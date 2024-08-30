#pragma once
#include "board.h"
#include "define.h"
namespace kc {
class Engine {
public:
    Engine();
    Move calc(const Board &chessBoard);
    int alphabeta(Board &board, int depth, Color color, int alpha, int beta);
private:
    Move bestMoves[256];
    int countBestMove = 0;
    int fixedDepth;

};
}
