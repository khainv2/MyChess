#include "engine.h"
#include "movegen.h"
#include "evaluation.h"
#include <random>
#include "util.h"
#include <array>
#include <cstring>
#include "QDateTime"

// Bật/tắt Quiescence Search để so sánh (1 = bật, 0 = tắt)
#define ENABLE_QUIESCENCE 1

// Bật/tắt Transposition Table (1 = bật, 0 = tắt)
#define ENABLE_TT 1

// Bật/tắt Random Move Selection khi có nhiều nước cùng điểm (1 = ngẫu nhiên, 0 = chọn nước đầu tiên)
#define ENABLE_RANDOM_BEST 1

// Bật/tắt Null Move Pruning (1 = bật, 0 = tắt)
#define ENABLE_NULL_MOVE 1
#define NULL_MOVE_R 2  // Depth reduction

// Bật/tắt Late Move Reduction (1 = bật, 0 = tắt)
#define ENABLE_LMR 1
#define LMR_FULL_MOVES 4   // Số nước đầu tìm đầy đủ
#define LMR_MIN_DEPTH 4    // Depth tối thiểu để áp dụng LMR

// Bật/tắt Principal Variation Search (1 = bật, 0 = tắt)
#define ENABLE_PVS 1

// Bật/tắt Iterative Deepening (1 = bật, 0 = tắt)
#define ENABLE_ID 1


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
    fixedDepth = 8;
}

/**
 * @brief Tính toán nước đi tốt nhất cho vị thế hiện tại
 * @param chessBoard Bàn cờ hiện tại
 * @return Nước đi tốt nhất được chọn
 */
Move kc::Engine::calc(const Board &chessBoard) {
    countBestMove = 0;
    Board board = chessBoard;
    board.initHash();
    int bestValue;

#if ENABLE_TT
    tt.clear();
#endif

    // Clear killer moves & history
    std::memset(killers, 0, sizeof(killers));
    std::memset(history, 0, sizeof(history));

#if ENABLE_ID
    // Iterative Deepening: tìm từ depth 1 → fixedDepth
    // TT tự động lưu best move từ depth trước → move ordering tốt hơn
    for (int d = 1; d <= fixedDepth; d++) {
        countBestMove = 0;
        // Không clear killers/history giữa các iteration — chúng vẫn hữu ích
        if (board.side == White){
            bestValue = negamax<White, true>(board, d, -Infinity, Infinity);
        } else {
            bestValue = negamax<Black, true>(board, d, -Infinity, Infinity);
        }
    }
#else
    // Gọi negamax trực tiếp ở fixedDepth
    if (board.side == White){
        bestValue = negamax<White, true>(board, fixedDepth, -Infinity, Infinity);
    } else {
        bestValue = negamax<Black, true>(board, fixedDepth, -Infinity, Infinity);
    }
#endif

#if ENABLE_RANDOM_BEST
    int r = getRand(0, countBestMove - 1);
    return bestMoves[r];
#else
    return bestMoves[0];
#endif
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
int Engine::negamax(Board &board, int depth, int alpha, int beta, bool allowNull){

    bool inCheck = board.anyCheck();

    // Điều kiện dừng: hết độ sâu tìm kiếm
    if (depth <= 0 && !inCheck) {
#if ENABLE_QUIESCENCE
        return quiescence<color>(board, alpha, beta);
#else
        return color ? -eval::estimate(board) : eval::estimate(board);
#endif
    }

#if ENABLE_TT
    // === TT Probe ===
    int origAlpha = alpha;
    u64 hash = board.hash;
    TTEntry ttEntry;
    u16 ttMoveRaw = 0;

    if (!isRoot && tt.probe(hash, ttEntry)) {
        if (ttEntry.depth >= depth) {
            int ttScore = ttEntry.score;
            if (ttEntry.flag == TT_EXACT) {
                return ttScore;
            } else if (ttEntry.flag == TT_LOWER_BOUND) {
                if (ttScore > alpha) alpha = ttScore;
            } else if (ttEntry.flag == TT_UPPER_BOUND) {
                if (ttScore < beta) beta = ttScore;
            }
            if (alpha >= beta) {
                return ttScore;
            }
        }
        // Lấy best move từ TT để ưu tiên trong move ordering
        ttMoveRaw = ttEntry.bestMove;
    }
#endif

#if ENABLE_NULL_MOVE
    // === Null Move Pruning ===
    if (!isRoot && allowNull && !inCheck && depth >= (1 + NULL_MOVE_R)) {
        BB myPieces = board.colors[color];
        BB bigPieces = myPieces & (board.types[Knight] | board.types[Bishop]
                                  | board.types[Rook] | board.types[Queen]);
        if (bigPieces) {
            BoardState nullState;
            board.doNullMove(nullState);
            int nullScore = -negamax<!color, false>(board, depth - 1 - NULL_MOVE_R, -beta, -beta + 1, false);
            board.undoNullMove();

            if (nullScore >= beta && nullScore > -ScoreMate + MaxPly && nullScore < ScoreMate - MaxPly) {
                return nullScore;
            }
        }
    }
#endif

    // Sinh tất cả nước đi hợp lệ
    auto movePtr = buff[depth];
    int count = MoveGen::instance->genMoveList(board, movePtr);

    // Không còn nước đi: chiếu hết hoặc hòa (stalemate)
    if (count == 0) {
        return inCheck ? -ScoreMate : 0;
    }

    // Gán điểm move ordering: TT move > Captures(MVV-LVA) > Killers > History > Quiet
    static constexpr int mvvlva[PieceType_NB] = { 0, 100, 320, 330, 500, 900, 0 };
    u16 killer0 = (depth < MaxPly) ? killers[depth][0].raw() : 0;
    u16 killer1 = (depth < MaxPly) ? killers[depth][1].raw() : 0;
    for (int i = 0; i < count; i++) {
        auto &m = movePtr[i];
#if ENABLE_TT
        if (m.raw() == ttMoveRaw && ttMoveRaw != 0) { m.val = 30000; continue; }
#endif
        if (m.is<Move::Capture>()) {
            // MVV-LVA: giá trị quân bị bắt - giá trị quân tấn công / 100
            Piece captured = board.pieces[m.dst()];
            PieceType capturedType = (captured != PieceNone) ? pieceToType(captured) : PieceTypeNone;
            PieceType attackerType = pieceToType(board.pieces[m.src()]);
            m.val = 10000 + mvvlva[capturedType] * 10 - mvvlva[attackerType];
        } else if (m.raw() == killer0 && killer0 != 0) {
            m.val = 9000;
        } else if (m.raw() == killer1 && killer1 != 0) {
            m.val = 8999;
        } else {
            // History heuristic: quiet moves hay gây cutoff được ưu tiên
            Piece pc = board.pieces[m.src()];
            m.val = history[pc][m.dst()];
        }
    }

    int bestValue = -Infinity;
    Move bestMove;
    BoardState state;

    // Duyệt qua tất cả nước đi (pick-best: chỉ đưa nước tốt nhất lên, không sort hết)
    for (int i = 0; i < count; i++){
        // Tìm nước có val cao nhất từ [i..count) và swap lên vị trí i
        for (int j = i + 1; j < count; j++) {
            if (movePtr[j].val > movePtr[i].val)
                std::swap(movePtr[i], movePtr[j]);
        }
        auto move = movePtr[i];
        board.doMove(move, state);

        int score;
#if ENABLE_PVS
        // PVS: nước đầu tiên tìm full window, các nước sau tìm zero-window trước
        // Không áp dụng PVS ở root (cần score chính xác cho mọi nước)
        if (isRoot || i == 0) {
            score = -negamax<!color, false>(board, depth - 1, -beta, -alpha);
        } else {
#if ENABLE_LMR
            int newDepth = depth - 1;
            // LMR: giảm depth cho nước quiet muộn
            if (i >= LMR_FULL_MOVES && depth >= LMR_MIN_DEPTH && !inCheck
                && !move.is<Move::Capture>() && !move.is<Move::Promotion>()
                && move.raw() != killer0 && move.raw() != killer1) {
                newDepth -= 1;
            }
            // Zero-window search
            score = -negamax<!color, false>(board, newDepth, -alpha - 1, -alpha);
#else
            // Zero-window search
            score = -negamax<!color, false>(board, depth - 1, -alpha - 1, -alpha);
#endif
            // Re-search full window nếu fail high
            if (score > alpha && score < beta) {
                score = -negamax<!color, false>(board, depth - 1, -beta, -alpha);
            }
        }
#else
        score = -negamax<!color, false>(board, depth - 1, -beta, -alpha);
#endif
        move.val = score;
        board.undoMove(move);

        // Cập nhật nước đi tốt nhất
        if (score > bestValue || (score == bestValue && bestValue == -Infinity)){
            if constexpr (isRoot){
                if (score > bestValue){
                    countBestMove = 0;
                }
                bestMoves[countBestMove++] = move;
            }
            bestValue = score;
            bestMove = move;

            if constexpr (isRoot){
                if (score > 30000){
                    break;
                }
            }
        }

        // Alpha-beta pruning
        alpha = std::max(alpha, score);
        if (alpha > beta) {
            // Lưu killer move + history (chỉ quiet moves)
            if (!move.is<Move::Capture>() && depth < MaxPly) {
                // Killer moves
                if (killers[depth][0].raw() != move.raw()) {
                    killers[depth][1] = killers[depth][0];
                    killers[depth][0] = move;
                }
                // History heuristic: tăng theo depth² (nước cắt ở depth cao quan trọng hơn)
                Piece pc = board.pieces[move.src()];
                history[pc][move.dst()] += depth * depth;
            }
            break;
        }
    }

#if ENABLE_TT
    // === TT Store ===
    TTFlag flag;
    if (bestValue <= origAlpha) {
        flag = TT_UPPER_BOUND;
    } else if (bestValue >= beta) {
        flag = TT_LOWER_BOUND;
    } else {
        flag = TT_EXACT;
    }
    tt.store(hash, bestValue, depth, flag, bestMove);
#endif

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

    // Fail-soft: theo dõi giá trị tốt nhất thực sự
    int bestValue = standPat;

    // Nếu stand-pat đã >= beta → cắt tỉa (vị thế đã quá tốt)
    if (standPat >= beta) {
        return standPat; // fail-soft: trả giá trị thực, không phải beta
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
            return -ScoreMate;
        }
        // Khi bị chiếu: sinh TẤT CẢ nước đi (không chỉ bắt quân)
        auto movePtr = qbuff[qdepth];
        int count = MoveGen::instance->genMoveList(board, movePtr);

        bestValue = -Infinity;
        BoardState state;
        for (int i = 0; i < count; i++) {
            auto move = movePtr[i];
            board.doMove(move, state);
            int score = -quiescence<!color>(board, -beta, -alpha, qdepth + 1);
            board.undoMove(move);
            if (score > bestValue) bestValue = score;
            if (score >= beta) return score; // fail-soft
            if (score > alpha) alpha = score;
        }
        return bestValue;
    }

    // Sinh CHỈ nước bắt quân + promotion
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

        if (score > bestValue) bestValue = score;
        if (score >= beta) {
            return score; // fail-soft: trả giá trị thực
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return bestValue;
}
