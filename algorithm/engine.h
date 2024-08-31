#pragma once
#include "board.h"
#include "define.h"
namespace kc {
class Engine {
public:
    Engine();
    Move calc(const Board &chessBoard);

    template <Color color, bool isRoot>
    int alphabeta(Board &board, int depth, int alpha, int beta);
private:
    Move bestMoves[256];
    int countBestMove = 0;
    int fixedDepth;

    Move engineMoves[20][256];
};
}
