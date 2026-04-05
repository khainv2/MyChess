# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Language Preference
Always respond in Vietnamese. When creating plans, save them in the `plans/` directory.

## Build Commands

**Quick build (recommended):** Run `build.bat` and select Debug or Release.

**Manual build:**
```powershell
# From project root
mkdir _build && cd _build
C:\Qt\Qt5.12.12\msvc2017_64\bin\qmake.exe ..\MyChess.pro
nmake debug    # or: nmake release
```

**Clean:** Run `clean.bat` or delete `_build/` directory.

**Run:** `_build\debug\MyChess.exe` or `_build\release\MyChess.exe`

## Architecture

This is a Qt 5.12.12 C++ chess application with an integrated chess engine.

### Chess Engine (`algorithm/`)
All engine code uses namespace `kc::`.

- **Board** (`board.h/cpp`): Bitboard-based board representation. `BB` = bitboard type. `BoardState` tracks castling rights, en passant, captured pieces. Uses template metaprogramming for color-generic code (`template<Color color>`).

- **MoveGen** (`movegen.h/cpp`): Legal move generation. Uses precomputed attack tables. Handles pins, checks, special moves (castling, en passant, promotion).

- **Attack** (`attack.h/cpp`): Precomputed attack tables. Must call `attack::init()` at startup.

- **Evaluation** (`evaluation.h/cpp`): Position evaluation with material values, piece-square tables, tapered eval (middlegame/endgame). Must call `eval::init()` at startup.

- **Engine** (`engine.h/cpp`): Negamax search with alpha-beta pruning. `Engine::calc()` returns best move. `Node` struct used for search tree visualization.

- **Perft** (`perft.h/cpp`): Move generation testing/debugging.

### GUI Components
- **ChessBoardView**: Custom QWidget for board display and interaction
- **MainWindow**: Main application window
- **PromotionSelectionDialog**: Pawn promotion piece selection

### Initialization Order
Required initialization in `main()`:
```cpp
attack::init();
MoveGen::init();
eval::init();
```

## Key Types (`algorithm/define.h`)
- `BB`: Bitboard (64-bit)
- `Square`: Board square (0-63)
- `Color`: White/Black
- `Piece`, `PieceType`: Piece definitions
- `Move`: Encoded move
- `CastlingRights`: Castling state flags
