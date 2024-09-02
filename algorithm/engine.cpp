#include "engine.h"
#include "movegen.h"
#include "evaluation.h"
#include <QElapsedTimer>
#include <QDebug>
#include <random>
#include "util.h"

#include <random>
#include "QDateTime"

int getRand(int min, int max){
    unsigned int ms = static_cast<unsigned>(QDateTime::currentMSecsSinceEpoch());
    std::mt19937 gen(ms);
    std::uniform_int_distribution<> uid(min, max);
    return uid(gen);
}

using namespace kc;
Engine::Engine()
{
    fixedDepth = 8;
    for (int i = 0; i < 256; i++){
        for (int j = 0; j < 256; j++){
            bestMoveHistories[i][j] = Move::NullMove;
        }
    }
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
    rootNode->score = eval::estimate(chessBoard);
//    rootNode->level = 0;
//    rootNode->board.state = new

    Board board = chessBoard;
//    BoardState state = *chessBoard.state;
//    board.state = &state;
//    *board.state = *chessBoard.state;
    int bestValue;
    if (board.side == White){
        bestValue = alphabeta<White, true>(rootNode, board, fixedDepth, -Infinity, Infinity);
    } else {
        bestValue = alphabeta<Black, true>(rootNode, board, fixedDepth, -Infinity, Infinity);
    }
    int r = getRand(0, countBestMove);
    qDebug() << "Best move count" << countBestMove << "Select" << r << "best value" << bestValue;
    for (int i = 0; i < countBestMove; i++){
        QString str;
        for (int j = 0; j < fixedDepth; j++){
            auto m = bestMoveHistories[i][j];
            str.append(QString::fromStdString(m.getDescription())).append(", ");
        }
        qDebug() << "- move list" << str;
    }

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

    if (depth == 0){
        countTotalSearch++;
        return color ? -eval::estimate(board) : eval::estimate(board);
//        return eval::estimate(board);
    }

    auto movePtr = engineMoves[depth];
    int count = MoveGen::instance->genMoveList(board, movePtr);

    int bestValue = -Infinity;
    BoardState state;
    for (int i = 0; i < count; i++){
        auto move = movePtr[i];
        auto child = new Node;
        node->children.push_back(child);
        child->move = move;
        board.doMove(move, state);
        int score = -alphabeta<!color, false>(child, board, depth - 1, -beta, -alpha);
        board.undoMove(move);
        child->score = score;
        if (score >= bestValue){
            moves[node->level] = move;
            if (depth == fixedDepth){
                if (score > bestValue){
                    countBestMove = 0;
                }
                memcpy(bestMoveHistories[countBestMove], moves, 256 * sizeof(Move));
                bestMoves[countBestMove++] = move;
            }
            bestValue = score;
        }

        if(score > alpha) alpha = score;
        if(alpha > beta) break;
    }

    return bestValue;

}
