"""
Test UCI engine against WAC (Win at Chess) 300 positions.
Usage: python test_wac.py [--engine PATH] [--depth N] [--time MS] [--epd FILE] [--workers N]
"""

import subprocess
import sys
import time
import re
import argparse
import csv
import os
import multiprocessing
import chess

# ── Config ──────────────────────────────────────────────────────────────────

DEFAULT_ENGINE = r"g:\Projects\Stockfish\stockfish\stockfish-windows-x86-64-avx2.exe"
DEFAULT_EPD = r"g:\Projects\MyChess\tests\wac.epd"
DEFAULT_DEPTH = 12


# ── EPD parsing ─────────────────────────────────────────────────────────────

def parse_epd_line(line: str):
    """Parse one EPD line, return (fen, best_moves_san, id)."""
    line = line.strip()
    if not line:
        return None

    parts = line.split()
    fen = " ".join(parts[:4])
    ops_str = " ".join(parts[4:])

    bm_match = re.search(r'bm\s+([^;]+);', ops_str)
    best_moves = []
    if bm_match:
        best_moves = [m.strip() for m in bm_match.group(1).split() if m.strip()]

    id_match = re.search(r'id\s+"([^"]+)"', ops_str)
    pos_id = id_match.group(1) if id_match else "?"

    return fen, best_moves, pos_id


# ── UCI engine communication ────────────────────────────────────────────────

class UCIEngine:
    def __init__(self, path: str):
        self.proc = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        self._send("uci")
        self._wait_for("uciok")
        self._send("setoption name Threads value 1")
        self._send("isready")
        self._wait_for("readyok")

    def _send(self, cmd: str):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def _wait_for(self, token: str, timeout: float = 60.0) -> list[str]:
        """Read lines until one starts with `token`. Return all lines read."""
        lines = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self.proc.stdout.readline().strip()
            if not line:
                continue
            lines.append(line)
            if line.startswith(token):
                return lines
        raise TimeoutError(f"Timeout waiting for '{token}'")

    def set_option(self, name: str, value):
        self._send(f"setoption name {name} value {value}")

    def go(self, fen: str, depth: int = 12, movetime: int = 0,
           timeout: float = 300.0) -> tuple[str, str]:
        """Send position + go, return (bestmove_uci, info_line_with_score)."""
        self._send("ucinewgame")
        self._send("isready")
        self._wait_for("readyok")

        full_fen = fen + " 0 1"
        self._send(f"position fen {full_fen}")

        if movetime > 0:
            self._send(f"go movetime {movetime}")
        else:
            self._send(f"go depth {depth}")

        lines = self._wait_for("bestmove", timeout=timeout)

        bestmove_line = [l for l in lines if l.startswith("bestmove")][0]
        bestmove_uci = bestmove_line.split()[1]

        score_line = ""
        for l in reversed(lines):
            if "score" in l and l.startswith("info"):
                score_line = l
                break

        return bestmove_uci, score_line

    def go_multipv(self, fen: str, depth: int = 12, movetime: int = 0,
                   multipv: int = 5, timeout: float = 300.0) -> list[dict]:
        """Search with MultiPV, return list of {rank, move, score, pv}."""
        self._send("ucinewgame")
        self.set_option("MultiPV", multipv)
        self._send("isready")
        self._wait_for("readyok")

        full_fen = fen + " 0 1"
        self._send(f"position fen {full_fen}")

        if movetime > 0:
            self._send(f"go movetime {movetime}")
        else:
            self._send(f"go depth {depth}")

        lines = self._wait_for("bestmove", timeout=timeout)

        # Keep only the LAST info line for each multipv rank (latest iteration wins)
        by_rank = {}
        for l in lines:
            if not l.startswith("info") or "multipv" not in l:
                continue
            rank_m = re.search(r'multipv (\d+)', l)
            if not rank_m:
                continue
            rank = int(rank_m.group(1))
            by_rank[rank] = l  # overwrite: last occurrence wins

        results = []
        for rank in sorted(by_rank.keys()):
            l = by_rank[rank]
            score_m = re.search(r'score (cp -?\d+|mate -?\d+)', l)
            pv_m = re.search(r' pv (.+)', l)

            score = score_m.group(1) if score_m else "?"
            pv = pv_m.group(1).strip() if pv_m else ""
            move = pv.split()[0] if pv else ""

            results.append({"rank": rank, "move": move, "score": score, "pv": pv})

        # Reset MultiPV
        self.set_option("MultiPV", 1)
        return results

    def quit(self):
        try:
            self._send("quit")
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()


# ── Move comparison ─────────────────────────────────────────────────────────

def san_to_uci(fen: str, san: str) -> str:
    """Convert SAN move to UCI notation using python-chess."""
    full_fen = fen + " 0 1"
    board = chess.Board(full_fen)
    try:
        move = board.parse_san(san)
        return move.uci()
    except (chess.InvalidMoveError, chess.IllegalMoveError):
        return ""


def extract_score(info_line: str) -> str:
    """Extract score from info line (e.g. 'cp 123' or 'mate 2')."""
    m = re.search(r'score\s+(cp\s+-?\d+|mate\s+-?\d+)', info_line)
    return m.group(1) if m else "?"


# ── Worker function (runs in separate process) ─────────────────────────────

_worker_engine = None
_worker_engine_path = None

def _worker_init(engine_path):
    """Initialize one engine per worker process."""
    global _worker_engine, _worker_engine_path
    _worker_engine_path = engine_path
    _worker_engine = UCIEngine(engine_path)

def _restart_engine():
    """Kill current engine and start a fresh one."""
    global _worker_engine, _worker_engine_path
    try:
        _worker_engine.proc.kill()
    except Exception:
        pass
    _worker_engine = UCIEngine(_worker_engine_path)

def solve_position(args_tuple):
    """Solve a single position using the worker's persistent engine. Retries once on failure."""
    fen, best_moves_san, pos_id, depth, movetime, index, timeout = args_tuple
    global _worker_engine

    for attempt in range(2):
        try:
            bestmove_uci, info_line = _worker_engine.go(fen, depth=depth, movetime=movetime, timeout=timeout)
            score = extract_score(info_line)

            expected_uci = []
            for san in best_moves_san:
                uci = san_to_uci(fen, san)
                if uci:
                    expected_uci.append(uci)

            is_correct = bestmove_uci in expected_uci
            status = "OK" if is_correct else "FAIL"

            return {
                "index": index,
                "id": pos_id,
                "fen": fen,
                "expected_san": ",".join(best_moves_san),
                "expected_uci": ",".join(expected_uci),
                "got_uci": bestmove_uci,
                "score": score,
                "status": status,
            }
        except Exception as e:
            if attempt == 0:
                _restart_engine()
                continue
            return {
                "index": index,
                "id": pos_id,
                "fen": fen,
                "expected_san": ",".join(best_moves_san),
                "expected_uci": "",
                "got_uci": "",
                "score": "?",
                "status": f"ERROR: {e}",
            }


# ── Main ────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Test UCI engine against WAC suite")
    parser.add_argument("--engine", default=DEFAULT_ENGINE, help="Path to UCI engine")
    parser.add_argument("--depth", type=int, default=DEFAULT_DEPTH, help="Search depth")
    parser.add_argument("--time", type=int, default=0, help="Movetime in ms (overrides depth)")
    parser.add_argument("--epd", default=DEFAULT_EPD, help="Path to EPD file")
    parser.add_argument("--csv", default="", help="Output CSV file for Qt viewer")
    parser.add_argument("--workers", "-w", type=int, default=1,
                        help="Number of parallel workers (each spawns its own engine)")
    parser.add_argument("--analyze", "-a", action="store_true",
                        help="Analyze FAIL positions with MultiPV to show why engine disagreed")
    parser.add_argument("--multipv", type=int, default=10,
                        help="Number of top moves to show in analyze mode (default: 10)")
    parser.add_argument("--id", type=str, default="",
                        help="Run only specific position(s), e.g. --id WAC.002 or --id 2 or --id 1-10")
    parser.add_argument("--timeout", type=int, default=300,
                        help="Timeout per position in seconds (default: 300)")
    args = parser.parse_args()

    # Load EPD
    with open(args.epd, "r") as f:
        lines = f.readlines()

    all_positions = []
    for line in lines:
        parsed = parse_epd_line(line)
        if parsed:
            all_positions.append(parsed)

    # Filter by --id if specified
    positions = all_positions
    if args.id:
        id_filter = args.id.strip()
        selected = set()

        for part in id_filter.split(","):
            part = part.strip()
            if "-" in part and not part.startswith("WAC"):
                # Range: "1-10"
                lo, hi = part.split("-", 1)
                for n in range(int(lo), int(hi) + 1):
                    selected.add(f"WAC.{n:03d}")
            elif part.isdigit():
                # Single number: "2"
                selected.add(f"WAC.{int(part):03d}")
            else:
                # Full ID: "WAC.002"
                selected.add(part)

        positions = [p for p in all_positions if p[2] in selected]

    total = len(positions)
    workers = max(1, min(args.workers, total))

    print(f"Engine : {args.engine}")
    print(f"EPD    : {args.epd} ({total} positions)")
    if args.time > 0:
        print(f"Mode   : movetime {args.time}ms")
    else:
        print(f"Mode   : depth {args.depth}")
    if args.id:
        print(f"Filter : {args.id}")
    print(f"Workers: {workers}")
    print("=" * 75)

    t_start = time.time()

    results = [None] * total

    if workers == 1:
        # Single-process mode: reuse one engine for all positions
        engine = UCIEngine(args.engine)
        for i, (fen, best_moves_san, pos_id) in enumerate(positions):
            try:
                bestmove_uci, info_line = engine.go(fen, depth=args.depth, movetime=args.time, timeout=args.timeout)
                score = extract_score(info_line)

                expected_uci = []
                for san in best_moves_san:
                    uci = san_to_uci(fen, san)
                    if uci:
                        expected_uci.append(uci)

                is_correct = bestmove_uci in expected_uci
                status = "OK" if is_correct else "FAIL"

                results[i] = {
                    "index": i,
                    "id": pos_id,
                    "fen": fen,
                    "expected_san": ",".join(best_moves_san),
                    "expected_uci": ",".join(expected_uci),
                    "got_uci": bestmove_uci,
                    "score": score,
                    "status": status,
                }
            except Exception as e:
                results[i] = {
                    "index": i,
                    "id": pos_id,
                    "fen": fen,
                    "expected_san": ",".join(best_moves_san),
                    "expected_uci": "",
                    "got_uci": "",
                    "score": "?",
                    "status": f"ERROR: {e}",
                }

            r = results[i]
            print(
                f"[{i+1:3d}/{total}] {r['id']:10s}  "
                f"expected={r['expected_san']:12s}  "
                f"got={r['got_uci']:7s}  "
                f"score={r['score']:12s}  "
                f"{r['status']}"
            )
        engine.quit()

    else:
        # Multi-process mode: each worker keeps one engine via initializer
        tasks = []
        for i, (fen, best_moves_san, pos_id) in enumerate(positions):
            tasks.append((fen, best_moves_san, pos_id, args.depth, args.time, i, args.timeout))

        completed = 0
        pool_timeout = args.timeout + 30  # extra margin over engine timeout
        with multiprocessing.Pool(
            processes=workers,
            initializer=_worker_init,
            initargs=(args.engine,),
        ) as pool:
            async_results = [pool.apply_async(solve_position, (t,)) for t in tasks]

            for i, ar in enumerate(async_results):
                try:
                    r = ar.get(timeout=pool_timeout)
                except multiprocessing.TimeoutError:
                    fen, best_moves_san, pos_id, *_ = tasks[i]
                    r = {
                        "index": i,
                        "id": pos_id,
                        "fen": fen,
                        "expected_san": ",".join(best_moves_san),
                        "expected_uci": "",
                        "got_uci": "",
                        "score": "?",
                        "status": "ERROR: pool timeout",
                    }

                results[r["index"]] = r
                completed += 1
                print(
                    f"[{completed:3d}/{total}] {r['id']:10s}  "
                    f"expected={r['expected_san']:12s}  "
                    f"got={r['got_uci']:7s}  "
                    f"score={r['score']:12s}  "
                    f"{r['status']}"
                )

            pool.terminate()

    elapsed = time.time() - t_start

    # Summary
    correct = sum(1 for r in results if r and r["status"] == "OK")
    failed = sum(1 for r in results if r and r["status"] == "FAIL")
    error_count = sum(1 for r in results if r and r["status"].startswith("ERROR"))

    print("=" * 75)
    print(f"Result : {correct}/{total} correct ({correct/total*100:.1f}%)")
    print(f"Failed : {failed}  |  Errors: {error_count}")
    print(f"Time   : {elapsed:.1f}s ({elapsed/total:.2f}s per position)")

    failed_list = [r for r in results if r and r["status"] == "FAIL"]
    if failed_list:
        print(f"\n--- Failed positions ({len(failed_list)}) ---")
        for r in sorted(failed_list, key=lambda x: x["index"]):
            print(f"  {r['id']:10s}  expected={r['expected_san']}  got={r['got_uci']}")

    # Analyze FAIL positions with MultiPV
    if args.analyze and failed_list:
        print(f"\n{'=' * 75}")
        print(f"ANALYZING {len(failed_list)} FAILED POSITIONS (MultiPV={args.multipv})")
        print(f"{'=' * 75}")

        analyzer = UCIEngine(args.engine)
        for r in sorted(failed_list, key=lambda x: x["index"]):
            fen = r["fen"]
            expected_uci_list = r["expected_uci"].split(",") if r["expected_uci"] else []

            pvs = analyzer.go_multipv(fen, depth=args.depth, movetime=args.time,
                                      multipv=args.multipv, timeout=args.timeout)

            # Convert UCI moves to SAN for readability
            full_fen = fen + " 0 1"
            board = chess.Board(full_fen)

            print(f"\n--- {r['id']} ---")
            print(f"  FEN:      {fen}")
            print(f"  Expected: {r['expected_san']} ({r['expected_uci']})")
            print(f"  Top {len(pvs)} moves:")

            expected_rank = None
            for pv in pvs:
                move_uci = pv["move"]
                try:
                    move_san = board.san(chess.Move.from_uci(move_uci))
                except Exception:
                    move_san = move_uci

                # Mark expected and engine's choice
                marker = "  "
                if move_uci in expected_uci_list:
                    marker = ">>"
                    expected_rank = pv["rank"]
                elif move_uci == r["got_uci"]:
                    marker = "**"

                # Show PV (first 6 moves)
                pv_short = " ".join(pv["pv"].split()[:6])
                print(f"    {marker} #{pv['rank']:2d}  {move_san:8s} ({move_uci:6s})  "
                      f"score={pv['score']:12s}  pv: {pv_short}")

            if expected_rank:
                print(f"  => Expected move ranked #{expected_rank}")
            else:
                print(f"  => Expected move NOT in top {args.multipv}")

        analyzer.quit()

    # Write CSV for Qt viewer
    csv_path = args.csv
    if not csv_path:
        csv_path = os.path.join(os.path.dirname(args.epd), "wac_results.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f, delimiter=";")
        writer.writerow(["id", "fen", "expected_san", "expected_uci", "got_uci", "score", "status"])
        for r in results:
            if r:
                writer.writerow([
                    r["id"], r["fen"], r["expected_san"],
                    r["expected_uci"], r["got_uci"], r["score"], r["status"],
                ])
    print(f"\nCSV    : {csv_path}")


if __name__ == "__main__":
    main()
