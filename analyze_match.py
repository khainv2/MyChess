#!/usr/bin/env python3
"""
Play one game MyChess vs Stockfish, then analyze every move with Stockfish
to find where MyChess made mistakes (biggest centipawn losses).

Usage: python analyze_match.py [--movetime 1000] [--sf-skill 10] [--mychess-white]
"""

import subprocess
import sys
import argparse

if sys.platform == "win32":
    MYCHESS = r"G:\Projects\MyChess\_build_uci\release\MyChessUCI.exe"
    STOCKFISH = r"G:\Projects\Stockfish\stockfish\stockfish-windows-x86-64-avx2.exe"
else:
    MYCHESS = "/home/dev2/test/MyChess/_build_uci/MyChessUCI"
    STOCKFISH = "/home/dev2/test/Stockfish/src/stockfish"


class UCIEngine:
    def __init__(self, path, name):
        self.name = name
        self.proc = subprocess.Popen(
            [path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, text=True, bufsize=1
        )
        self._send("uci")
        self._wait_for("uciok")

    def _send(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def _wait_for(self, token):
        while True:
            line = self.proc.stdout.readline().strip()
            if line.startswith(token):
                return line

    def set_option(self, name, value):
        self._send(f"setoption name {name} value {value}")

    def isready(self):
        self._send("isready")
        self._wait_for("readyok")

    def newgame(self):
        self._send("ucinewgame")
        self.isready()

    def go(self, moves, movetime=1000, depth=0):
        if moves:
            self._send(f"position startpos moves {' '.join(moves)}")
        else:
            self._send("position startpos")
        self.isready()
        if depth > 0:
            self._send(f"go depth {depth}")
        else:
            self._send(f"go movetime {movetime}")
        line = self._wait_for("bestmove")
        return line.split()[1]

    def analyze(self, moves, depth=18):
        """Analyze position, return (score_cp, best_move, pv)."""
        if moves:
            self._send(f"position startpos moves {' '.join(moves)}")
        else:
            self._send("position startpos")
        self.isready()
        self._send(f"go depth {depth}")

        score_cp = 0
        best_move = ""
        pv = ""
        mate = None

        while True:
            line = self.proc.stdout.readline().strip()
            if line.startswith("bestmove"):
                best_move = line.split()[1]
                break
            if "score" in line and "depth" in line:
                parts = line.split()
                # Parse depth
                if "depth" in parts:
                    d_idx = parts.index("depth")
                # Parse score
                if "score cp" in line:
                    idx = parts.index("cp")
                    score_cp = int(parts[idx + 1])
                    mate = None
                elif "score mate" in line:
                    idx = parts.index("mate")
                    mate = int(parts[idx + 1])
                    score_cp = 30000 if mate > 0 else -30000
                # Parse PV
                if "pv" in parts:
                    pv_idx = parts.index("pv")
                    pv = " ".join(parts[pv_idx + 1:])

        return score_cp, mate, best_move, pv

    def quit(self):
        try:
            self._send("quit")
            self.proc.wait(timeout=3)
        except:
            self.proc.kill()


def play_game(movetime, sf_skill, mychess_white):
    """Play one game, return list of moves and winner."""
    mychess = UCIEngine(MYCHESS, "MyChess")
    stockfish = UCIEngine(STOCKFISH, "Stockfish")
    stockfish.set_option("Skill Level", sf_skill)
    stockfish.set_option("Threads", 1)
    stockfish.set_option("Hash", 16)

    if mychess_white:
        engines = [mychess, stockfish]
        names = ["MyChess", "Stockfish"]
    else:
        engines = [stockfish, mychess]
        names = ["Stockfish", "MyChess"]

    print(f"  Playing: {names[0]} (White) vs {names[1]} (Black)")
    print(f"  Movetime: {movetime}ms, SF Skill: {sf_skill}")

    moves = []
    move_num = 1
    winner = None

    while len(moves) < 400:
        turn = len(moves) % 2
        eng = engines[turn]
        try:
            bestmove = eng.go(moves, movetime=movetime)
        except:
            winner = names[1 - turn]
            break

        if bestmove in ("(none)", "0000", "none"):
            winner = names[1 - turn]
            break

        if len(bestmove) >= 4 and bestmove[:2] == bestmove[2:4]:
            winner = names[1 - turn]
            break

        side = "W" if turn == 0 else "B"
        if turn == 0:
            print(f"  {move_num}. {bestmove}", end="", flush=True)
        else:
            print(f" {bestmove}", flush=True)
            move_num += 1
        moves.append(bestmove)

    if len(moves) % 2 == 1:
        print()  # newline after incomplete move pair

    mychess.quit()
    stockfish.quit()

    if winner is None:
        winner = "draw"
    print(f"\n  Result: {winner} wins ({len(moves)} moves)\n")
    return moves, names, winner


def analyze_game(moves, names):
    """Analyze each move with Stockfish depth 18. Find MyChess mistakes."""
    print("=" * 70)
    print("  ANALYSIS (Stockfish depth 18)")
    print("  Score from White's perspective. CPL = centipawn loss per move.")
    print("=" * 70)

    analyzer = UCIEngine(STOCKFISH, "Analyzer")
    analyzer.set_option("Threads", 2)
    analyzer.set_option("Hash", 64)
    analyzer.isready()

    prev_score = 0  # score from white's perspective
    mistakes = []  # (move_idx, move, cpl, prev_score, new_score, best_move)

    for i in range(len(moves)):
        turn = i % 2  # 0=white, 1=black
        move_played = moves[i]
        engine_name = names[turn]

        # Analyze position AFTER this move
        score_cp, mate, best_move, pv = analyzer.analyze(moves[:i + 1], depth=18)

        # Score from white's perspective
        white_score = score_cp if (i + 1) % 2 == 0 else -score_cp
        # If it's after white's move (turn==0), next to move is black, so score is from black's POV
        # Actually: after moves[:i+1], it's the opponent's turn
        # Stockfish returns score from side-to-move's perspective
        # After move i (turn), side to move is (1-turn)
        # So: white_score = score if (1-turn)==White else -score
        if turn == 0:
            # White just moved, Black to move -> score is from Black's POV
            white_score = -score_cp
        else:
            # Black just moved, White to move -> score is from White's POV
            white_score = score_cp

        # Centipawn loss: how much worse is this move compared to the best?
        # For White: loss = prev_score - white_score (positive = bad)
        # For Black: loss = white_score - prev_score (positive = bad)
        if turn == 0:  # White moved
            cpl = prev_score - white_score
        else:  # Black moved
            cpl = white_score - prev_score

        move_num = i // 2 + 1
        side = "W" if turn == 0 else "B"

        # Classify
        label = ""
        if mate is not None:
            label = f"  MATE in {abs(mate)}"
        elif cpl >= 300:
            label = "  ??? BLUNDER"
        elif cpl >= 100:
            label = "  ?  MISTAKE"
        elif cpl >= 50:
            label = "  ?! INACCURACY"

        if cpl >= 50 or mate is not None:
            prefix = f"  {move_num:3d}{'.' if turn == 0 else '...'}"
            print(f"{prefix} {move_played:6s} [{engine_name:10s}]  "
                  f"eval: {white_score/100:+.2f}  cpl: {cpl:+4d}{label}")

            if engine_name == "MyChess" and cpl >= 50:
                mistakes.append((i, move_played, cpl, prev_score, white_score, best_move))

        prev_score = white_score

    analyzer.quit()

    # Summary
    print(f"\n{'='*70}")
    print(f"  MYCHESS MISTAKES SUMMARY (cpl >= 50)")
    print(f"{'='*70}")

    if not mistakes:
        print("  No significant mistakes found!")
    else:
        total_cpl = 0
        mychess_moves = 0
        for i in range(len(moves)):
            if names[i % 2] == "MyChess":
                mychess_moves += 1

        for idx, move, cpl, prev_sc, new_sc, best in mistakes:
            move_num = idx // 2 + 1
            side = "." if idx % 2 == 0 else "..."
            label = "BLUNDER" if cpl >= 300 else "MISTAKE" if cpl >= 100 else "INACCURACY"
            print(f"  {move_num}{side}{move:6s}  cpl:{cpl:+4d}  "
                  f"({prev_sc/100:+.2f} -> {new_sc/100:+.2f})  "
                  f"best: {best}  [{label}]")
            total_cpl += cpl

        avg_cpl = total_cpl / mychess_moves if mychess_moves else 0
        print(f"\n  Total CPL: {total_cpl}")
        print(f"  MyChess moves: {mychess_moves}")
        print(f"  Average CPL: {avg_cpl:.1f}")
        print(f"  Blunders (>=300): {sum(1 for _,_,c,_,_,_ in mistakes if c >= 300)}")
        print(f"  Mistakes (>=100): {sum(1 for _,_,c,_,_,_ in mistakes if 100 <= c < 300)}")
        print(f"  Inaccuracies (>=50): {sum(1 for _,_,c,_,_,_ in mistakes if 50 <= c < 100)}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--movetime', type=int, default=1000)
    p.add_argument('--sf-skill', type=int, default=10)
    p.add_argument('--mychess-white', action='store_true', default=True)
    p.add_argument('--mychess-black', action='store_true')
    args = p.parse_args()

    mychess_white = not args.mychess_black

    moves, names, winner = play_game(args.movetime, args.sf_skill, mychess_white)

    if len(moves) > 4:
        analyze_game(moves, names)
    else:
        print("  Game too short to analyze.")


if __name__ == "__main__":
    main()
