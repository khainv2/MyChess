#include "engine.h"
#include "movegenerator.h"
#include "evaluation.h"
#include <QElapsedTimer>
#include <QDebug>
#include <random>
#include "util.h"

using namespace kchess;
Engine::Engine()
{
    fixedDepth = 6;
}

Move kchess::Engine::calc(const ChessBoard &chessBoard)
{
    countBestMove = 0;
    QElapsedTimer timer;
    timer.start();
    alphabeta(chessBoard, fixedDepth, getColorActive(chessBoard.state), -Infinity, Infinity);
    int r = countBestMove * rand() / RAND_MAX;
    qDebug() << "Best move count" << countBestMove << "Select" << r;
    Move move = bestMoves[r];
//    Move move = bestMoves[0];
    qDebug() << "Select best move" << getMoveDescription(move).c_str();
    qDebug() << "Time to calculate" << (timer.nsecsElapsed() / 1000000);
    return move;
}

int Engine::alphabeta(const ChessBoard &chessBoard, int depth, Color color, int alpha, int beta){
    if (depth == 0){
        return eval::estimate(chessBoard);
    }

    Move moves[256];
    int count;
    generateMoveList(chessBoard, moves, count);

    if (color == White){
        int max = -Infinity;
        for (int i = 0; i < count; i++){
            ChessBoard t = chessBoard;
            t.doMove(moves[i]);
            int val = alphabeta(t, depth - 1, toggleColor(color), alpha, beta);
//            qDebug() << "Depth" << depth << val << getMoveDescription(moves[i]).c_str() << toFenString(t);
            if (val >= max){
                if (depth == fixedDepth){
                    if (val > max){
                        countBestMove = 0;
                    }
                    bestMoves[countBestMove++] = moves[i];
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
        for (int i = 0; i < count; i++){
            ChessBoard t = chessBoard;
            t.doMove(moves[i]);
            int val = alphabeta(t, depth - 1, toggleColor(color), alpha, beta);
//            qDebug() << "Depth" << depth << val << getMoveDescription(moves[i]).c_str() << toFenString(t);

            if (val <= min){
                if (depth == fixedDepth){
                    if (val < min){
                        countBestMove = 0;
                    }
                    bestMoves[countBestMove++] = moves[i];
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
