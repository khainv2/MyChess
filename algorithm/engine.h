#pragma once
#include "board.h"
#include "define.h"
#include "tt.h"

namespace kc {

class Engine {
    enum {
        MaxPly = 32,
    };
public:
    Engine();
    Move calc(const Board &chessBoard);

    template <Color color, bool isRoot>
    int negamax(Board &board, int depth, int alpha, int beta, bool allowNull = true);

    template <Color color>
    int quiescence(Board &board, int alpha, int beta, int qdepth = 0);

private:
    static constexpr int MaxQDepth = 32;
    Move bestMoves[256];
    int countBestMove = 0;
    int fixedDepth;

    Move bestMoveHistories[256][MaxPly];
    int bestMoveHistoryCount[256] = { 0 };
    Move moves[MaxPly];
    int movePlyCount = 0;
    Move buff[MaxPly * 2][256];
    Move qbuff[MaxQDepth][64];

    // Killer moves: 2 killers per depth
    static constexpr int NumKillers = 2;
    Move killers[MaxPly][NumKillers];

    // History heuristic: quiet move gây cutoff bao nhiêu lần [piece][toSquare]
    int history[Piece_NB][Square_NB];

    TranspositionTable tt;
};
}
