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
    fixedDepth = 2;
}

int countTotalSearch = 0;
Move kc::Engine::calc(const Board &chessBoard)
{
    countBestMove = 0;
    QElapsedTimer timer;
    timer.start();
    countTotalSearch = 0;

    if (rootNode != nullptr){
        delete rootNode;
    }
    rootNode = new Node;
//    rootNode->level = 0;
//    rootNode->board.state = new

    Board board = chessBoard;
//    BoardState state = *chessBoard.state;
//    board.state = &state;
//    *board.state = *chessBoard.state;
    if (board.side == White){
        alphabeta<White, true>(rootNode, board, fixedDepth, -Infinity, Infinity);
    } else {
        alphabeta<Black, true>(rootNode, board, fixedDepth, -Infinity, Infinity);
    }
    int r = countBestMove * rand() / RAND_MAX;
    qDebug() << "Best move count" << countBestMove << "Select" << r;
    Move move = bestMoves[r];
//    Move move = bestMoves[0];
    qDebug() << "Select best move" << move.getDescription().c_str();
    qDebug() << "Total move search..." << countTotalSearch;
    qDebug() << "Time to calculate" << (timer.nsecsElapsed() / 1000000);
    return move;
}

Node *Engine::getRootNode() const
{
    return rootNode;
}

template <Color color, bool isRoot>
int Engine::alphabeta(Node *node, Board &board, int depth, int alpha, int beta){
    node->level = fixedDepth - depth;
    node->board = board;
    node->boardState = *board.state;
    node->board.state = &node->boardState; // Self control state, note: value previous is not valid

//    auto child = new Node;
//    if constexpr (isRoot){
//        node.
//    }

    if (depth == 0){
        countTotalSearch++;
        return eval::estimate(board);
    }

    auto movePtr = engineMoves[depth];
    int count = MoveGen::instance->genMoveList(board, movePtr);

    if constexpr (color == White){
        int max = -Infinity;
        BoardState state;
        for (int i = 0; i < count; i++){
            auto move = movePtr[i];
            auto child = new Node;
            node->children.push_back(child);
            child->move = move;
            board.doMove(move, state);
            int val = alphabeta<!color, false>(child, board, depth - 1, alpha, beta);
            board.undoMove(move);
            if (val >= max){
                if (depth == fixedDepth){
                    if (val > max){
                        countBestMove = 0;
                    }
                    bestMoves[countBestMove++] = move;
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
            auto move = movePtr[i];
            auto child = new Node;
            node->children.push_back(child);
            child->move = move;
            board.doMove(move, state);
            int val = alphabeta<!color, false>(child, board, depth - 1, alpha, beta);
            board.undoMove(move);
            if (val <= min){
                if (depth == fixedDepth){
                    if (val < min){
                        countBestMove = 0;
                    }
                    bestMoves[countBestMove++] = move;
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
