#pragma once
#include "chessboard.h"
#include "define.h"
namespace kchess {
class Engine {
public:
    Engine();
    Move calc(const ChessBoard &chessBoard);
    int alphabeta(const ChessBoard &chessBoard, int depth, Color color, int alpha, int beta);
private:
    Move bestMoves[256];
    int countBestMove = 0;
    int fixedDepth;

};
}
