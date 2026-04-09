#!/usr/bin/env python3
"""
Engine vs Engine match via UCI protocol.
Usage: python3 match.py [--games N] [--movetime MS] [--depth D]
"""

import subprocess
import sys
import time
import argparse

MYCHESS = "/home/dev2/test/MyChess/_build_uci/MyChessUCI"
STOCKFISH = "/home/dev2/test/Stockfish/src/stockfish"

PIECE_UNICODE = {
    'K': '\u2654', 'Q': '\u2655', 'R': '\u2656', 'B': '\u2657', 'N': '\u2658', 'P': '\u2659',
    'k': '\u265a', 'q': '\u265b', 'r': '\u265c', 'b': '\u265d', 'n': '\u265e', 'p': '\u265f',
}

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--games', type=int, default=2, help='Number of games (default 2, alternating colors)')
    p.add_argument('--movetime', type=int, default=500, help='Time per move in ms (default 500)')
    p.add_argument('--depth', type=int, default=0, help='Fixed depth (0=use movetime)')
    p.add_argument('--sf-skill', type=int, default=0, help='Stockfish Skill Level 0-20 (default 0)')
    return p.parse_args()

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

    def go(self, moves, movetime=500, depth=0):
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

    def quit(self):
        try:
            self._send("quit")
            self.proc.wait(timeout=3)
        except:
            self.proc.kill()


def fen_to_board(moves):
    """Simulate board from startpos + moves list. Returns 8x8 grid."""
    board = [
        ['R','N','B','Q','K','B','N','R'],
        ['P','P','P','P','P','P','P','P'],
        ['.','.','.','.','.','.','.','.'],
        ['.','.','.','.','.','.','.','.'],
        ['.','.','.','.','.','.','.','.'],
        ['.','.','.','.','.','.','.','.'],
        ['p','p','p','p','p','p','p','p'],
        ['r','n','b','q','k','b','n','r'],
    ]

    side = 'w'
    castling = {'K': True, 'Q': True, 'k': True, 'q': True}
    ep_square = None

    for uci in moves:
        sf = ord(uci[0]) - ord('a')
        sr = int(uci[1]) - 1
        df = ord(uci[2]) - ord('a')
        dr = int(uci[3]) - 1
        promo = uci[4] if len(uci) == 5 else None

        piece = board[sr][sf]
        captured = board[dr][df]
        board[dr][df] = piece
        board[sr][sf] = '.'

        # En passant capture
        if piece.lower() == 'p' and sf != df and captured == '.':
            board[sr][df] = '.'

        # Castling move
        if piece == 'K' and sf == 4 and df == 6:  # White O-O
            board[0][7] = '.'; board[0][5] = 'R'
        elif piece == 'K' and sf == 4 and df == 2:  # White O-O-O
            board[0][0] = '.'; board[0][3] = 'R'
        elif piece == 'k' and sf == 4 and df == 6:  # Black O-O
            board[7][7] = '.'; board[7][5] = 'r'
        elif piece == 'k' and sf == 4 and df == 2:  # Black O-O-O
            board[7][0] = '.'; board[7][3] = 'r'

        # Promotion
        if promo:
            if side == 'w':
                board[dr][df] = promo.upper()
            else:
                board[dr][df] = promo.lower()

        # Update castling rights
        if piece == 'K': castling['K'] = castling['Q'] = False
        if piece == 'k': castling['k'] = castling['q'] = False
        if piece == 'R' and sr == 0 and sf == 7: castling['K'] = False
        if piece == 'R' and sr == 0 and sf == 0: castling['Q'] = False
        if piece == 'r' and sr == 7 and sf == 7: castling['k'] = False
        if piece == 'r' and sr == 7 and sf == 0: castling['q'] = False

        side = 'b' if side == 'w' else 'w'

    return board, side


def print_board(moves, white_name, black_name, move_num):
    board, side = fen_to_board(moves)

    print(f"\n  {black_name} (Black)")
    print("  +---+---+---+---+---+---+---+---+")
    for r in range(7, -1, -1):
        row = f"{r+1} |"
        for f in range(8):
            p = board[r][f]
            ch = PIECE_UNICODE.get(p, ' ')
            row += f" {ch} |"
        print(row)
        print("  +---+---+---+---+---+---+---+---+")
    print("    a   b   c   d   e   f   g   h")
    print(f"  {white_name} (White)")

    turn = "White" if side == 'w' else "Black"
    print(f"\n  Move {move_num} | Turn: {turn}")
    if moves:
        print(f"  Last move: {moves[-1]}")


def is_game_over(moves):
    """Simple draw detection: 3-fold repetition or too many moves."""
    if len(moves) >= 400:  # 200 moves each = likely draw
        return True, "draw (too many moves)"
    return False, None


def play_game(white_engine, black_engine, movetime, depth):
    white_engine.newgame()
    black_engine.newgame()

    moves = []
    engines = [white_engine, black_engine]
    move_num = 1

    print_board(moves, white_engine.name, black_engine.name, move_num)

    while True:
        turn = len(moves) % 2  # 0=white, 1=black
        eng = engines[turn]

        try:
            bestmove = eng.go(moves, movetime=movetime, depth=depth)
        except Exception as e:
            winner = engines[1 - turn].name
            print(f"\n  {eng.name} crashed! {winner} wins.")
            return winner

        if bestmove in ("(none)", "0000", "none"):
            # Engine has no legal move = checkmate or stalemate
            winner = engines[1 - turn].name
            print(f"\n  {eng.name} has no move. {winner} wins!")
            return winner

        # Detect invalid move (src == dst)
        if len(bestmove) >= 4 and bestmove[:2] == bestmove[2:4]:
            winner = engines[1 - turn].name
            print(f"\n  {eng.name} returned invalid move '{bestmove}'. {winner} wins!")
            return winner

        moves.append(bestmove)
        if turn == 0:
            move_num += 1

        print_board(moves, white_engine.name, black_engine.name, move_num)

        over, reason = is_game_over(moves)
        if over:
            print(f"\n  Game over: {reason}")
            return "draw"


def main():
    args = parse_args()

    print("=" * 50)
    print("  MyChess vs Stockfish")
    print(f"  Games: {args.games}")
    if args.depth > 0:
        print(f"  Depth: {args.depth}")
    else:
        print(f"  Time per move: {args.movetime}ms")
    print(f"  Stockfish Skill: {args.sf_skill}")
    print("=" * 50)

    results = {"MyChess": 0, "Stockfish": 0, "draw": 0}

    for game_num in range(1, args.games + 1):
        print(f"\n{'='*50}")
        print(f"  Game {game_num}/{args.games}")

        mychess = UCIEngine(MYCHESS, "MyChess")
        stockfish = UCIEngine(STOCKFISH, "Stockfish")
        stockfish.set_option("Skill Level", args.sf_skill)
        stockfish.set_option("Threads", 1)
        stockfish.set_option("Hash", 16)

        # Alternate colors
        if game_num % 2 == 1:
            print("  MyChess = White, Stockfish = Black")
            white, black = mychess, stockfish
        else:
            print("  Stockfish = White, MyChess = Black")
            white, black = stockfish, mychess

        winner = play_game(white, black, args.movetime, args.depth)

        if winner == "draw":
            results["draw"] += 1
        else:
            results[winner] += 1

        mychess.quit()
        stockfish.quit()

        print(f"\n  Score: MyChess {results['MyChess']} - Stockfish {results['Stockfish']} - Draws {results['draw']}")

    print(f"\n{'='*50}")
    print(f"  FINAL: MyChess {results['MyChess']} - Stockfish {results['Stockfish']} - Draws {results['draw']}")
    print("=" * 50)


if __name__ == "__main__":
    main()
