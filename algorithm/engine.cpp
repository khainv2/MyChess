#include "engine.h"
#include "movegen.h"
#include "evaluation.h"
#include <QElapsedTimer>
#include <QDebug>
#include <random>
#include "util.h"
#include <cmath>
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
    fixedDepth = 5;
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

    countBestMove = 0;
    Board board = chessBoard;
    int bestValue;
    if (board.side == White){
        bestValue = negamax<White, true>(board, fixedDepth, -Infinity, Infinity);
    } else {
        bestValue = negamax<Black, true>(board, fixedDepth, -Infinity, Infinity);
    }
    int r = getRand(0, countBestMove - 1);
    qDebug() << "Best move count" << countBestMove << "Select" << r << "best value" << bestValue;
    for (int i = 0; i < countBestMove; i++){
        QString str;
        for (int j = 0; j < fixedDepth; j++){
            auto m = bestMoveHistories[i][j];
            str.append(QString::fromStdString(m.getDescription())).append("|").append(QString::number(m.val))
                    .append(", ");

        }
        qDebug() << "- move list" << str << "move ply count" << bestMoveHistoryCount[i];
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
int Engine::negamax(Board &board, int depth, int alpha, int beta){
    int level = fixedDepth - depth;
    if (board.isMate()){
        countTotalSearch++;
        movePlyCount = level + 1;
        return color ? -ScoreMate : ScoreMate;
    }

    if (depth == 0){
        countTotalSearch++;
        movePlyCount = level + 1;
        return color ? -eval::estimate(board) : eval::estimate(board);
    }

    auto movePtr = buff[depth];
    int count = MoveGen::instance->genMoveList(board, movePtr);

    int bestValue = -Infinity;
    BoardState state;
    for (int i = 0; i < count; i++){
        auto move = movePtr[i];
        if constexpr (isRoot){
            qDebug() << "start calculate for" << move.getDescription().c_str();
        } else {
//            qDebug() << tab << move.getDescription().c_str();
        }
        board.doMove(move, state);
        int score = -negamax<!color, false>(board, depth - 1, -beta, -alpha);
        move.val = score;
        board.undoMove(move);
//        child->score = score;
        if (score >= bestValue){
            moves[level] = move;
//            memset(moves + level, 0, (MaxPly - level) * sizeof(Move));
            if (depth == fixedDepth){
                if (score > bestValue){
                    // Reset toàn bộ biến đếm các nước đi tốt nhất, trong trường hợp tìm thấy điểm cao hơn
                    countBestMove = 0;
                }
                memcpy(bestMoveHistories[countBestMove], moves, MaxPly * sizeof(Move));
                bestMoveHistoryCount[countBestMove] = movePlyCount;
                bestMoves[countBestMove] = move;
                countBestMove++;
            }
            bestValue = score;
            // Ngừng tìm kiếm nếu thấy chiếu hết
            if (score > 30000){
//                break;
//                qDebug() << "Found mate" << board.getPrintable().c_str();
            }
        }

        alpha = std::max(alpha, score);
        if (alpha > beta)
            break;
    }

    return bestValue;

}
