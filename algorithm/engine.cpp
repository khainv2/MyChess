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
Engine::Engine(){
    fixedDepth = 6;
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
        qDebug() << "-" << bestMoves[i].getDescription().c_str();
    }

    Move move = bestMoves[r];
//    Move move = bestMoves[0];
    qDebug() << r << bestMoves[r].flag();
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

    if (board.anyCheck()) {
        if (board.isMate()){
            countTotalSearch++;
            return color ? -ScoreMate : ScoreMate;
        }
    }

    if (depth == 0) {
        countTotalSearch++;
        return color ? -eval::estimate(board) : eval::estimate(board);
    }

    auto movePtr = buff[depth];
    int count = MoveGen::instance->genMoveList(board, movePtr);

    if constexpr (isRoot){
        //
        std::vector<Move> moveData(count);
        auto moveDataPtr = moveData.data();
        // Perform insertion sort

        for (int i = 0; i < count; i++){
            moveData[i] = movePtr[i];
        }



        std::sort(moveData.begin(), moveData.begin() + count, [=](Move m1, Move m2){
            if (m1.is<Move::Capture>() && !m2.is<Move::Capture>()){
                return true;
            } else if (!m1.is<Move::Capture>() && m2.is<Move::Capture>()){
                return false;
            }
            return m1.val > m2.val;
        });

        for (int i = 0; i < count; i++){
            movePtr[i] = moveData[i];
        }
    }

    int bestValue = -Infinity;
    BoardState state;
    for (int i = 0; i < count; i++){
        auto move = movePtr[i];
        if constexpr (isRoot){
            qDebug() << "start calculate for" << move.getDescription().c_str();
        }
        board.doMove(move, state);
        int score = -negamax<!color, false>(board, depth - 1, -beta, -alpha);
        move.val = score;
        board.undoMove(move);

        // Thêm điều kiện =, cho phép kiểm tra nhiều nước đi cùng lúc
        if (score >= bestValue){
            if constexpr (isRoot){
                if (score > bestValue){
                    // Reset toàn bộ biến đếm các nước đi tốt nhất, trong trường hợp tìm thấy điểm cao hơn
                    countBestMove = 0;
                }
                bestMoves[countBestMove++] = move;

            }
            bestValue = score;

            if constexpr (isRoot){
                // Ngừng tìm kiếm nếu thấy chiếu hết
                if (score > 30000){
                    break;
                }
            }
        }

        alpha = std::max(alpha, score);
        if (alpha > beta)
            break;
    }

    return bestValue;

}
