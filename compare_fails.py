"""
So sánh kết quả engine với Stockfish trên các bài WAC bị FAIL.
Usage: python compare_fails.py [--max-time 500] [--depth 15]
"""
import subprocess
import csv
import sys
import os
import threading

STOCKFISH_PATH = r"G:\Projects\Stockfish\stockfish\stockfish-windows-x86-64-avx2.exe"
CSV_PATH = "wac_console_results.csv"
SF_DEPTH = 15
MAX_TIME_MS = 0

for i in range(1, len(sys.argv)):
    if sys.argv[i] == "--max-time" and i + 1 < len(sys.argv):
        MAX_TIME_MS = int(sys.argv[i + 1])
    if sys.argv[i] == "--depth" and i + 1 < len(sys.argv):
        SF_DEPTH = int(sys.argv[i + 1])


class StockfishSession:
    """Giữ 1 process Stockfish chạy suốt, gửi nhiều lệnh qua UCI."""
    def __init__(self, path):
        self.proc = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            text=True, bufsize=1
        )
        self._send("uci")
        self._wait_for("uciok")
        self._send("isready")
        self._wait_for("readyok")

    def _send(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def _wait_for(self, token):
        while True:
            line = self.proc.stdout.readline().strip()
            if line.startswith(token):
                return line

    def analyse(self, fen, depth):
        self._send(f"position fen {fen} 0 1")
        self._send(f"go depth {depth}")

        best_move = ""
        score = ""
        pv = ""
        depth_reached = 0

        while True:
            line = self.proc.stdout.readline().strip()
            if line.startswith("bestmove"):
                best_move = line.split()[1]
                break
            if "info depth" in line and " pv " in line and "upperbound" not in line and "lowerbound" not in line:
                parts = line.split()
                try:
                    d = int(parts[parts.index("depth") + 1])
                    # Bỏ qua seldepth lines chỉ lấy main depth
                    if d > depth_reached:
                        depth_reached = d
                        if "score cp" in line:
                            si = parts.index("cp")
                            score = f"{int(parts[si+1])}cp"
                        elif "score mate" in line:
                            si = parts.index("mate")
                            score = f"mate {parts[si+1]}"
                        pv_idx = parts.index("pv")
                        pv = " ".join(parts[pv_idx+1:min(pv_idx+6, len(parts))])
                except (ValueError, IndexError):
                    pass

        return best_move, score, pv, depth_reached

    def close(self):
        self._send("quit")
        self.proc.wait(timeout=5)


def main():
    if not os.path.exists(CSV_PATH):
        print(f"ERROR: {CSV_PATH} not found. Run build_console.bat first.")
        return
    if not os.path.exists(STOCKFISH_PATH):
        print(f"ERROR: Stockfish not found at {STOCKFISH_PATH}")
        return

    # Read CSV
    fails = []
    with open(CSV_PATH, "r") as f:
        reader = csv.DictReader(f, delimiter=";")
        for row in reader:
            if row["status"] == "FAIL":
                time_ms = int(row["time_ms"])
                if MAX_TIME_MS == 0 or time_ms <= MAX_TIME_MS:
                    fails.append(row)

    if not fails:
        print("No FAIL entries found matching criteria.")
        return

    print(f"{'='*80}")
    print(f"  WAC FAIL Analysis - {len(fails)} positions")
    print(f"  Stockfish depth: {SF_DEPTH}")
    if MAX_TIME_MS > 0:
        print(f"  Filter: time <= {MAX_TIME_MS}ms")
    print(f"{'='*80}\n")

    sf = StockfishSession(STOCKFISH_PATH)
    verdicts = {"ENGINE_WRONG": 0, "EPD_DEBATABLE": 0, "ALL_DIFFER": 0}

    for row in fails:
        fen = row["fen"]
        wac_id = row["id"]
        expected = row["expected_san"]
        expected_uci = row["expected_uci"]
        got_uci = row["got_uci"]
        time_ms = row["time_ms"]

        sf_move, sf_score, sf_pv, sf_depth = sf.analyse(fen, SF_DEPTH)

        engine_match_sf = (got_uci == sf_move)
        sf_match_expected = (sf_move in expected_uci.split(",")) if expected_uci else False

        if sf_match_expected:
            tag = "ENGINE_WRONG"
        elif engine_match_sf:
            tag = "EPD_DEBATABLE"
        else:
            tag = "ALL_DIFFER"
        verdicts[tag] += 1

        print(f"--- {wac_id} ({time_ms}ms) [{tag}] ---")
        print(f"  FEN:       {fen}")
        print(f"  Expected:  {expected} ({expected_uci})")
        print(f"  MyChess:   {got_uci}")
        print(f"  Stockfish: {sf_move} ({sf_score}, d={sf_depth})  PV: {sf_pv}")
        print()

    sf.close()

    print(f"{'='*80}")
    print(f"  Summary:")
    for v, c in sorted(verdicts.items()):
        if c > 0:
            print(f"    {v}: {c}")
    total = len(fails)
    real_wrong = verdicts["ENGINE_WRONG"]
    print(f"\n  Total FAIL: {total}")
    print(f"  Engine thuc su sai: {real_wrong}")
    print(f"  EPD debatable / all differ: {total - real_wrong}")
    print(f"{'='*80}")


if __name__ == "__main__":
    main()
