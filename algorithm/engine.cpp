#include "engine.h"
#include "movegen.h"
#include "evaluation.h"
#include <QElapsedTimer>
#include <QDebug>
#include <random>
#include "util.h"

using namespace kc;
Engine::Engine()
{
    fixedDepth = 8;
}

int countTotalSearch = 0;
Move kc::Engine::calc(const Board &chessBoard)
{
    countBestMove = 0;
    QElapsedTimer timer;
    timer.start();
    countTotalSearch = 0;
    Board board = chessBoard;
    *board.state = *chessBoard.state;
    alphabeta(board, fixedDepth, chessBoard.side, -Infinity, Infinity);
    int r = countBestMove * rand() / RAND_MAX;
    qDebug() << "Best move count" << countBestMove << "Select" << r;
    Move move = bestMoves[r];
//    Move move = bestMoves[0];
    qDebug() << "Select best move" << move.getDescription().c_str();
    qDebug() << "Total move search..." << countTotalSearch;
    qDebug() << "Time to calculate" << (timer.nsecsElapsed() / 1000000);
    return move;
}

Move engineMoves[20][256];
int Engine::alphabeta(Board &board, int depth, Color color, int alpha, int beta){
    if (depth == 0){
        countTotalSearch++;
        return eval::estimate(board);
    }

    auto movePtr = engineMoves[depth];
    int count = MoveGen::instance->genMoveList(board, movePtr);

    if (color == White){
        int max = -Infinity;
        BoardState state;
        for (int i = 0; i < count; i++){
            board.doMove(movePtr[i], state);
            int val = alphabeta(board, depth - 1, !color, alpha, beta);
            board.undoMove(movePtr[i]);
            if (val >= max){
                if (depth == fixedDepth){
                    if (val > max){
                        countBestMove = 0;
                    }
                    bestMoves[countBestMove++] = movePtr[i];
                }
                max = val;
            }
            if (alpha < max){
                alpha = max;
            }
            if (max >= beta){
                break;
            }
        }
        return max;
    } else {
        int min = Infinity;
        BoardState state;
        for (int i = 0; i < count; i++){
            board.doMove(movePtr[i], state);
            int val = alphabeta(board, depth - 1, !color, alpha, beta);
            board.undoMove(movePtr[i]);
            if (val <= min){
                if (depth == fixedDepth){
                    if (val < min){
                        countBestMove = 0;
                    }
                    bestMoves[countBestMove++] = movePtr[i];
                }
                min = val;
            }
            if (beta > min){
                beta = min;
            }
            if (min <= alpha){
                break;
            }
        }
        return min;
    }
}
