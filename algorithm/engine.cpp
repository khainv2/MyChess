#include "engine.h"
#include "movegen.h"
#include "evaluation.h"
#include <QElapsedTimer>
#include <QDebug>
#include <random>
#include "util.h"
#include <cmath>
#include <random>
#include <array>
#include "QDateTime"


#define DEBUG_ENGINE true
/**
 * @brief Sinh số ngẫu nhiên trong khoảng [min, max]
 * @param min Giá trị nhỏ nhất
 * @param max Giá trị lớn nhất
 * @return Số ngẫu nhiên trong khoảng cho trước
 */
int getRand(int min, int max){
    unsigned int ms = static_cast<unsigned>(QDateTime::currentMSecsSinceEpoch());
    std::mt19937 gen(ms);
    std::uniform_int_distribution<> uid(min, max);
    return uid(gen);
}

using namespace kc;

/**
 * @brief Constructor của Engine - khởi tạo độ sâu tìm kiếm mặc định
 */
Engine::Engine(){
    fixedDepth = 7;
}

int countTotalSearch = 0;

/**
 * @brief Tính toán nước đi tốt nhất cho vị thế hiện tại
 * @param chessBoard Bàn cờ hiện tại
 * @return Nước đi tốt nhất được chọn
 */
Move kc::Engine::calc(const Board &chessBoard) {
    countBestMove = 0;
    QElapsedTimer timer;
    timer.start();
    countTotalSearch = 0;

    // Xóa root node cũ nếu có
    if (rootNode != nullptr){
        delete rootNode;
    }
    countBestMove = 0;
    Board board = chessBoard;
    int bestValue;
    
    // Gọi negamax tùy theo màu quân đang đi
    if (board.side == White){
        bestValue = negamax<White, true>(board, fixedDepth, -Infinity, Infinity);
    } else {
        bestValue = negamax<Black, true>(board, fixedDepth, -Infinity, Infinity);
    }
    
    // Chọn ngẫu nhiên một nước đi trong số các nước đi tốt nhất
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

/**
 * @brief Getter cho root node của cây tìm kiếm
 * @return Pointer đến root node
 */
Node *Engine::getRootNode() const
{
    return rootNode;
}

/**
 * @brief Thuật toán Negamax với Alpha-Beta pruning
 * @tparam color Màu quân đang đi (White/Black)
 * @tparam isRoot Có phải là node gốc hay không
 * @param board Bàn cờ hiện tại
 * @param depth Độ sâu tìm kiếm còn lại
 * @param alpha Giá trị alpha cho pruning
 * @param beta Giá trị beta cho pruning
 * @return Điểm đánh giá tốt nhất cho vị thế hiện tại
 */
template <Color color, bool isRoot>
int Engine::negamax(Board &board, int depth, int alpha, int beta){

    #if DEBUG_ENGINE
    if (depth >= fixedDepth - 2) { // Chỉ debug 2 level đầu để tránh quá nhiều log
        qDebug() << QString("=== NEGAMAX: Color=%1, Depth=%2, Alpha=%3, Beta=%4 ===")
                    .arg(color ? "Black" : "White")
                    .arg(depth)
                    .arg(alpha)
                    .arg(beta);
    }
    #endif

    // Kiểm tra chiếu và chiếu hết
    if (board.anyCheck()) {
        if (board.isMate()){
            countTotalSearch++;
            #if DEBUG_ENGINE
            if (depth >= fixedDepth - 2) {
                qDebug() << QString("MATE FOUND: Color=%1, Score=%2")
                            .arg(color ? "Black" : "White")
                            .arg(color ? -ScoreMate : ScoreMate);
            }
            #endif
            return color ? -ScoreMate : ScoreMate;
        }
    }

    // Điều kiện dừng: hết độ sâu tìm kiếm
    if (depth == 0) {
        countTotalSearch++;
        int evalScore = color ? -eval::estimate(board) : eval::estimate(board);
        #if DEBUG_ENGINE
        qDebug() << QString("LEAF NODE: Color=%1, Eval=%2")
                    .arg(color ? "Black" : "White")
                    .arg(evalScore);
        #endif
        return evalScore;
    }

    // Sinh tất cả nước đi hợp lệ
    auto movePtr = buff[depth];
    int count = MoveGen::instance->genMoveList(board, movePtr);

    #if DEBUG_ENGINE
    if (depth >= fixedDepth - 2) {
        qDebug() << QString("MOVE GEN: Found %1 moves at depth %2").arg(count).arg(depth);
    }
    #endif

    // Sắp xếp nước đi: capture moves trước, sau đó theo giá trị
    std::sort(movePtr, movePtr + count, [=](Move m1, Move m2){
        if (m1.is<Move::Capture>() && !m2.is<Move::Capture>()){
            return true;
        } else if (!m1.is<Move::Capture>() && m2.is<Move::Capture>()){
            return false;
        }
        return m1.val > m2.val;
    });

    int bestValue = -Infinity;
    BoardState state;
    
    #if DEBUG_ENGINE
    if (depth >= fixedDepth - 2) {
        qDebug() << QString("MOVE ORDER: Top 3 moves:");
        for (int i = 0; i < std::min(3, count); i++) {
            qDebug() << QString("  %1: %2 (val=%3, capture=%4)")
                        .arg(i+1)
                        .arg(movePtr[i].getDescription().c_str())
                        .arg(movePtr[i].val)
                        .arg(movePtr[i].is<Move::Capture>() ? "YES" : "NO");
        }
    }
    #endif
    
    // Duyệt qua tất cả nước đi
    for (int i = 0; i < count; i++){
        auto move = movePtr[i];
        if constexpr (isRoot){
            qDebug() << "start calculate for" << move.getDescription().c_str();
        }
        
        // Thực hiện nước đi
        board.doMove(move, state);
        int score = -negamax<!color, false>(board, depth - 1, -beta, -alpha);
        move.val = score;
        board.undoMove(move);

        #if DEBUG_ENGINE
        if (depth >= fixedDepth - 2) {
            qDebug() << QString("MOVE EVAL: %1 -> Score=%2 (Alpha=%3, Beta=%4)")
                        .arg(move.getDescription().c_str())
                        .arg(score)
                        .arg(alpha)
                        .arg(beta);
        }
        #endif

        // Cập nhật nước đi tốt nhất
        if (score >= bestValue){
            if constexpr (isRoot){
                if (score > bestValue){
                    // Reset toàn bộ biến đếm các nước đi tốt nhất, trong trường hợp tìm thấy điểm cao hơn
                    countBestMove = 0;
                    #if DEBUG_ENGINE
                    qDebug() << QString("NEW BEST SCORE: %1 (better than %2)").arg(score).arg(bestValue);
                    #endif
                }
                bestMoves[countBestMove++] = move;
                #if DEBUG_ENGINE
                qDebug() << QString("BEST MOVE ADDED: %1 (count=%2)").arg(move.getDescription().c_str()).arg(countBestMove);
                #endif
            }
            bestValue = score;

            if constexpr (isRoot){
                // Ngừng tìm kiếm nếu thấy chiếu hết
                if (score > 30000){
                    #if DEBUG_ENGINE
                    qDebug() << "MATE SCORE FOUND - STOPPING SEARCH";
                    #endif
                    break;
                }
            }
        }

        // Alpha-beta pruning
        int oldAlpha = alpha;
        alpha = std::max(alpha, score);
        if (alpha > beta) {
            #if DEBUG_ENGINE
            if (depth >= fixedDepth - 2) {
                qDebug() << QString("ALPHA-BETA CUTOFF: Alpha=%1 > Beta=%2 at move %3")
                            .arg(alpha)
                            .arg(beta)
                            .arg(move.getDescription().c_str());
            }
            #endif
            break;
        }
        
        #if DEBUG_ENGINE
        if (depth >= fixedDepth - 2 && alpha > oldAlpha) {
            qDebug() << QString("ALPHA IMPROVED: %1 -> %2").arg(oldAlpha).arg(alpha);
        }
        #endif
    }

    #if DEBUG_ENGINE
    if (depth >= fixedDepth - 2) {
        qDebug() << QString("NEGAMAX RESULT: Depth=%1, BestValue=%2")
                    .arg(depth)
                    .arg(bestValue);
    }
    #endif

    return bestValue;

}
