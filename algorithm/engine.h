#pragma once
#include "board.h"
#include "define.h"

namespace kc {

class Engine {
    enum {
        MaxPly = 32,
    };
public:
    Engine();
    Move calc(const Board &chessBoard);

    template <Color color, bool isRoot>
    int negamax(Board &board, int depth, int alpha, int beta);

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
    Move buff[20][256];
    Move qbuff[MaxQDepth][64]; // Buffer cho quiescence (captures ít, 64 là đủ)
};
}
