#include "engine.h"
#include "movegen.h"
#include "evaluation.h"
#include <random>
#include "util.h"
#include <array>
#include "QDateTime"

// Bật/tắt Quiescence Search để so sánh (1 = bật, 0 = tắt)
#define ENABLE_QUIESCENCE 1


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
    fixedDepth = 4;
}

/**
 * @brief Tính toán nước đi tốt nhất cho vị thế hiện tại
 * @param chessBoard Bàn cờ hiện tại
 * @return Nước đi tốt nhất được chọn
 */
Move kc::Engine::calc(const Board &chessBoard) {
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
    Move move = bestMoves[r];
    return move;
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

    // Kiểm tra chiếu và chiếu hết
    if (board.anyCheck()) {
        if (board.isMate()){
            return color ? -ScoreMate : ScoreMate;
        }
    }

    // Điều kiện dừng: hết độ sâu tìm kiếm
    if (depth == 0) {
#if ENABLE_QUIESCENCE
        return quiescence<color>(board, alpha, beta);
#else
        return color ? -eval::estimate(board) : eval::estimate(board);
#endif
    }

    // Sinh tất cả nước đi hợp lệ
    auto movePtr = buff[depth];
    int count = MoveGen::instance->genMoveList(board, movePtr);

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

    // Duyệt qua tất cả nước đi
    for (int i = 0; i < count; i++){
        auto move = movePtr[i];
        // Thực hiện nước đi
        board.doMove(move, state);
        int score = -negamax<!color, false>(board, depth - 1, -beta, -alpha);
        move.val = score;
        board.undoMove(move);

        // Cập nhật nước đi tốt nhất
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

        // Alpha-beta pruning
        alpha = std::max(alpha, score);
        if (alpha > beta) {
            break;
        }
    }

    return bestValue;

}

/**
 * @brief Quiescence Search - tìm kiếm thêm chỉ các nước bắt quân
 * cho đến khi vị thế "yên tĩnh" (không còn bắt quân có lợi)
 * Giải quyết horizon effect: tránh đánh giá sai khi đang giữa chuỗi đổi quân
 */
template <Color color>
int Engine::quiescence(Board &board, int alpha, int beta, int qdepth) {
    // Đánh giá tĩnh (stand-pat): chỉ material + PST, bỏ mobility cho nhanh
    int standPat = color ? -eval::estimateFast(board) : eval::estimateFast(board);

    // Nếu stand-pat đã >= beta → cắt tỉa (vị thế đã quá tốt)
    if (standPat >= beta) {
        return beta;
    }

    // Cập nhật alpha nếu stand-pat tốt hơn
    if (standPat > alpha) {
        alpha = standPat;
    }

    // Giới hạn độ sâu quiescence để tránh explosion
    if (qdepth >= MaxQDepth) {
        return standPat;
    }

    // Kiểm tra chiếu hết
    if (board.anyCheck()) {
        if (board.isMate()) {
            return color ? -ScoreMate : ScoreMate;
        }
    }

    // Sinh CHỈ nước bắt quân + promotion (dùng buffer pre-allocated)
    auto movePtr = qbuff[qdepth];
    int count = MoveGen::instance->genCaptureMoveList(board, movePtr);

    // MVV-LVA: gán val = giá trị quân bị bắt, sort giảm dần
    static constexpr int pieceValues[] = { 0, 100, 320, 330, 500, 900, 0, 0,
                                           0, 100, 320, 330, 500, 900, 0, 0 };
    for (int i = 0; i < count; i++) {
        Piece captured = board.pieces[movePtr[i].dst()];
        movePtr[i].val = (captured != PieceNone) ? pieceValues[captured] : 0;
        if (movePtr[i].is<Move::Promotion>()) movePtr[i].val += 800;
    }
    std::sort(movePtr, movePtr + count, [](Move m1, Move m2) {
        return m1.val > m2.val;
    });

    BoardState state;

    for (int i = 0; i < count; i++) {
        auto move = movePtr[i];

        // Delta pruning: bỏ qua nếu material gain + margin vẫn < alpha
        if (!move.is<Move::Promotion>()) {
            if (standPat + move.val + 200 < alpha) {
                continue;
            }
        }

        board.doMove(move, state);
        int score = -quiescence<!color>(board, -beta, -alpha, qdepth + 1);
        board.undoMove(move);

        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return alpha;
}
