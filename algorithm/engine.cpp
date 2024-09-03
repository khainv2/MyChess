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
    countBestMove = 0;
    Board board = chessBoard;
    Move bestValue;
    if (board.side == White){
        bestValue = negamax<White, true>(board, fixedDepth, -Infinity, Infinity);
    } else {
        bestValue = negamax<Black, true>(board, fixedDepth, -Infinity, Infinity);
    }
    int r = getRand(0, countBestMove - 1);
    qDebug() << "Best move count" << countBestMove << "Select" << r << "best value" << bestValue.val;
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
Move Engine::negamax(Board &board, int depth, int alpha, int beta){
//    if constexpr
    int level = fixedDepth - depth;
    if (board.isMate()){
        countTotalSearch++;
        movePlyCount = level + 1;
        Move m;
        m.val = color ? -ScoreMate : ScoreMate;
        return m;
    }

    if (depth == 0){
        countTotalSearch++;
        movePlyCount = level + 1;
        Move m;
        m.val = color ? -eval::estimate(board) : eval::estimate(board);
        return m;
    }

    auto movePtr = buff[depth];
    int count = MoveGen::instance->genMoveList(board, movePtr);

    if constexpr (isRoot){
        std::vector<Move> moveData(count);
        for (int i = 0; i < count; i++){
            moveData[i] = movePtr[i];
        }
        std::sort(moveData.begin(), moveData.begin() + count, [=](Move m1, Move m2){
            // If move is F5>G6, always return true

//            if (m1.getDescription() == "F5>G6"){
//                return true;
//            }
            
//            if (m2.getDescription() == "F5>G6"){
//                return false;
//            }

            return m1.val > m2.val;
        });

        for (int i = 0; i < count; i++){
            movePtr[i] = moveData[i];
        }
    }

//    int bestValue = -Infinity;
    BoardState state;
    Move m;
    m.val = -Infinity;
    for (int i = 0; i < count; i++){
        auto move = movePtr[i];
        if constexpr (isRoot){
            qDebug() << "start calculate for" << move.getDescription().c_str();
        } else {
//            qDebug() << tab << move.getDescription().c_str();
        }
        board.doMove(move, state);
        Move bestChildMove = negamax<!color, false>(board, depth - 1, -beta, -alpha);
        int score = -bestChildMove.val;
        move.val = score;
        board.undoMove(move);

        // Thêm điều kiện =, cho phép kiểm tra nhiều nước đi cùng lúc
        if (score >= m.val){
            moves[level] = move;
            if constexpr (isRoot){
                if (score > m.val){
                    // Reset toàn bộ biến đếm các nước đi tốt nhất, trong trường hợp tìm thấy điểm cao hơn
                    countBestMove = 0;
                }
                memcpy(bestMoveHistories[countBestMove], moves, MaxPly * sizeof(Move));
                bestMoveHistoryCount[countBestMove] = movePlyCount;
                bestMoves[countBestMove] = move;
                countBestMove++;
                // Ngừng tìm kiếm nếu thấy chiếu hết
//                if (score > 30000){
//                    break;
//                }
            }
            m = move;
            m.val = score;
        }

        alpha = std::max(alpha, score);
        if (alpha > beta)
            break;
    }

    return m;

}
