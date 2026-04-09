#include "engine.h"
#include "movegen.h"
#include "evaluation.h"
#include "attack.h"
#include <random>
#include "util.h"
#include <array>
#include <cstring>
#include <chrono>
#include <algorithm>

static constexpr int NULL_MOVE_R = 2;           // Null Move: depth reduction
static constexpr int LMR_FULL_MOVES = 4;        // LMR: số nước đầu tìm đầy đủ
static constexpr int LMR_MIN_DEPTH = 4;          // LMR: depth tối thiểu để áp dụng
static constexpr int ASPIRATION_WINDOW = 50;      // Aspiration: kích thước cửa sổ ban đầu (centipawns)
static constexpr int ASPIRATION_MIN_DEPTH = 4;    // Aspiration: chỉ dùng từ depth này trở lên


/**
 * @brief Sinh số ngẫu nhiên trong khoảng [min, max]
 * @param min Giá trị nhỏ nhất
 * @param max Giá trị lớn nhất
 * @return Số ngẫu nhiên trong khoảng cho trước
 */
int getRand(int min, int max){
    unsigned int seed = static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> uid(min, max);
    return uid(gen);
}

using namespace kc;

// Giá trị quân cho SEE
static constexpr int SeeValues[PieceType_NB] = { 0, 100, 320, 330, 500, 900, 20000 };

/**
 * @brief Static Exchange Evaluation - đánh giá chuỗi bắt quân tại một ô
 * Trả về material gain/loss nếu cả hai bên bắt quân tối ưu
 */
static int see(const Board &board, Move move) {
    int from = move.src();
    int to = move.dst();

    PieceType attacker = pieceToType(board.pieces[from]);
    Piece capturedPc = board.pieces[to];
    PieceType captured = (capturedPc != PieceNone) ? pieceToType(capturedPc) : PieceTypeNone;

    // En passant: bắt tốt
    if (move.is<Move::Enpassant>()) captured = Pawn;

    int gain[32];
    int depth = 0;

    gain[depth] = SeeValues[captured];

    // Promotion: thêm giá trị quân mới - tốt
    if (move.is<Move::Promotion>()) {
        gain[depth] += SeeValues[move.getPromotionPieceType()] - SeeValues[Pawn];
        attacker = move.getPromotionPieceType(); // Quân trên ô đích giờ là quân promote
    }

    BB occ = board.getOccupancy();

    // Bỏ quân tấn công khỏi occupancy
    occ ^= indexToBB(from);

    // En passant: bỏ tốt bị bắt
    if (move.is<Move::Enpassant>()) {
        Color atkColor = pieceToColor(board.pieces[from]);
        int epSq = atkColor == White ? to - 8 : to + 8;
        occ ^= indexToBB(epSq);
    }

    // Lấy tất cả quân tấn công ô đích (sliding pieces cập nhật theo occ mới)
    BB attackers = board.getSqAttackTo(to, occ) & occ;

    Color sideToMove = !pieceToColor(board.pieces[from]);
    PieceType lastCaptured = attacker;

    while (true) {
        depth++;
        gain[depth] = SeeValues[lastCaptured] - gain[depth - 1];

        // Pruning: nếu cả 2 bên đều không cải thiện được, dừng
        if (std::max(-gain[depth - 1], gain[depth]) < 0) break;

        // Tìm quân tấn công rẻ nhất của bên đang đi
        BB sideAttackers = attackers & board.colors[sideToMove];
        if (!sideAttackers) break;

        PieceType lva = PieceTypeNone;
        BB fromBB = 0;
        for (int pt = Pawn; pt <= King; pt++) {
            BB candidates = sideAttackers & board.types[pt];
            if (candidates) {
                lva = PieceType(pt);
                fromBB = lsbBB(candidates);
                break;
            }
        }
        if (lva == PieceTypeNone) break;

        // Bỏ quân này khỏi occupancy
        occ ^= fromBB;

        // Phát hiện quân trượt mới (X-ray) đằng sau quân vừa bỏ
        if (lva == Pawn || lva == Bishop || lva == Queen) {
            attackers |= attack::getBishopAttacks(to, occ) & (board.types[Bishop] | board.types[Queen]);
        }
        if (lva == Rook || lva == Queen) {
            attackers |= attack::getRookAttacks(to, occ) & (board.types[Rook] | board.types[Queen]);
        }
        attackers &= occ;

        lastCaptured = lva;
        sideToMove = !sideToMove;
    }

    // Unwind negamax
    while (--depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
    }

    return gain[0];
}

/**
 * @brief Constructor của Engine - khởi tạo độ sâu tìm kiếm mặc định
 */
Engine::Engine(){
    fixedDepth = 9;
}

/**
 * @brief Phân bổ thời gian cho nước đi dựa trên TimeInfo
 *
 * Chiến lược phân bổ:
 * - moveTime: dùng trực tiếp, soft = 90%, hard = 100%
 * - time control: phân bổ dựa trên thời gian còn lại / số nước ước tính
 *   + soft limit: thời gian mong muốn (kết thúc iteration nếu vượt)
 *   + hard limit: thời gian tối đa (abort search nếu vượt)
 */
void Engine::allocateTime(const TimeInfo &timeInfo) {
    if (timeInfo.moveTimeMs > 0) {
        // Chế độ move time cố định
        softLimitMs = timeInfo.moveTimeMs * 90 / 100;  // 90% để an toàn
        hardLimitMs = timeInfo.moveTimeMs;
        useTimeControl = true;
        return;
    }

    if (timeInfo.remainingMs <= 0) {
        // Không có thông tin thời gian → không dùng time control
        useTimeControl = false;
        return;
    }

    useTimeControl = true;

    // Ước tính số nước còn lại trong ván
    int movesToGo = timeInfo.movesToGo > 0 ? timeInfo.movesToGo : 30;

    // Thời gian cơ bản cho mỗi nước
    int baseTime = timeInfo.remainingMs / movesToGo;

    // Cộng thêm phần increment (dùng ~80% increment)
    int incBonus = timeInfo.incrementMs * 80 / 100;

    // Soft limit: thời gian mong muốn cho nước này
    // Không vượt quá 25% thời gian còn lại
    softLimitMs = std::min(baseTime + incBonus,
                           timeInfo.remainingMs * 25 / 100);

    // Hard limit: tối đa gấp 3 lần soft limit
    // Không vượt quá 70% thời gian còn lại (giữ buffer)
    hardLimitMs = std::min(softLimitMs * 3,
                           timeInfo.remainingMs * 70 / 100);

    // Đảm bảo tối thiểu 10ms
    softLimitMs = std::max(softLimitMs, 10);
    hardLimitMs = std::max(hardLimitMs, 10);
}

/**
 * @brief Kiểm tra có nên dừng search không (gọi mỗi CheckTimeInterval nodes)
 * @return true nếu đã vượt hard limit
 */
bool Engine::shouldStop() {
    if (!useTimeControl) return false;
    return elapsedMs() >= hardLimitMs;
}

/**
 * @brief Thời gian đã trôi qua kể từ khi bắt đầu search (ms)
 */
int Engine::elapsedMs() const {
    auto now = Clock::now();
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - searchStart).count()
    );
}

/**
 * @brief Tính toán nước đi tốt nhất (chế độ fixed depth - backward compatible)
 * @param chessBoard Bàn cờ hiện tại
 * @return Nước đi tốt nhất được chọn
 */
Move kc::Engine::calc(const Board &chessBoard) {
    TimeInfo ti;
    ti.depth = fixedDepth;
    return calc(chessBoard, ti);
}

/**
 * @brief Tính toán nước đi tốt nhất với quản lý thời gian
 * @param chessBoard Bàn cờ hiện tại
 * @param timeInfo Thông tin thời gian (depth, move time, hoặc time control)
 * @return Nước đi tốt nhất được chọn
 */
Move kc::Engine::calc(const Board &chessBoard, const TimeInfo &timeInfo) {
    countBestMove = 0;
    completedDepth = 0;
    bestScore = 0;
    nodeCount = 0;
    searchAborted = false;
    Board board = chessBoard;
    board.initHash();
    int bestValue;

    tt.clear();

    // Clear killer moves & history
    std::memset(killers, 0, sizeof(killers));
    std::memset(history, 0, sizeof(history));

    // Khởi tạo time management
    searchStart = Clock::now();
    allocateTime(timeInfo);

    // Kiểm tra nếu không còn nước đi hợp lệ (checkmate/stalemate)
    {
        auto movePtr = buff[0];
        int count = MoveGen::instance->genMoveList(board, movePtr);
        if (count == 0) {
            bestScore = board.anyCheck() ? -ScoreMate : 0;
            return Move(); // Không còn nước đi
        }
    }

    // Xác định depth tối đa
    int maxDepth = (timeInfo.depth > 0) ? timeInfo.depth : MaxDepthLimit;

    // Lưu best move từ iteration trước (phòng trường hợp abort giữa chừng)
    Move lastCompletedBestMove;
    int lastCompletedCount = 0;
    Move lastCompletedMoves[256];
    PVLine lastCompletedPv;

    PVLine pv;

    // Iterative Deepening: tìm từ depth 1 → maxDepth
    // TT tự động lưu best move từ depth trước → move ordering tốt hơn
    bestValue = 0;
    for (int d = 1; d <= maxDepth; d++) {
        // Kiểm tra soft limit TRƯỚC khi reset countBestMove
        // (tránh mất kết quả từ iteration trước)
        if (useTimeControl && d > 1 && elapsedMs() >= softLimitMs) {
            break;
        }

        countBestMove = 0;

        int prevBestValue = bestValue;

        // Aspiration Windows: dùng score từ depth trước để thu hẹp cửa sổ
        if (d >= ASPIRATION_MIN_DEPTH) {
            int alpha = bestValue - ASPIRATION_WINDOW;
            int beta  = bestValue + ASPIRATION_WINDOW;
            int delta = ASPIRATION_WINDOW;

            while (true) {
                if (board.side == White){
                    bestValue = negamax<White, true>(board, d, alpha, beta, pv);
                } else {
                    bestValue = negamax<Black, true>(board, d, alpha, beta, pv);
                }

                // Search bị abort → dùng kết quả từ iteration trước
                if (searchAborted) break;

                // Nếu score nằm trong cửa sổ → xong
                if (bestValue > alpha && bestValue < beta) break;

                // Fail-low: score < alpha → mở rộng alpha
                if (bestValue <= alpha) {
                    alpha = std::max(bestValue - delta, -Infinity);
                }
                // Fail-high: score >= beta → mở rộng beta
                if (bestValue >= beta) {
                    beta = std::min(bestValue + delta, Infinity);
                }

                delta *= 2;  // Mở rộng gấp đôi mỗi lần fail

                // Nếu cửa sổ đã quá rộng → full window search
                if (alpha <= -Infinity && beta >= Infinity) break;

                countBestMove = 0;  // Reset best moves cho lần search lại
            }
        } else {
            // Depth thấp: full window search (chưa có score tham chiếu đáng tin cậy)
            if (board.side == White){
                bestValue = negamax<White, true>(board, d, -Infinity, Infinity, pv);
            } else {
                bestValue = negamax<Black, true>(board, d, -Infinity, Infinity, pv);
            }
        }

        // Nếu search bị abort giữa iteration → khôi phục kết quả trước
        if (searchAborted) {
            if (lastCompletedCount > 0) {
                countBestMove = lastCompletedCount;
                std::memcpy(bestMoves, lastCompletedMoves, sizeof(Move) * lastCompletedCount);
                bestValue = prevBestValue;
                pv = lastCompletedPv;
            }
            break;
        }

        // Lưu kết quả iteration hoàn chỉnh
        completedDepth = d;
        lastCompletedCount = countBestMove;
        std::memcpy(lastCompletedMoves, bestMoves, sizeof(Move) * countBestMove);
        lastCompletedPv = pv;

        // UCI info callback
        if (infoCallback) {
            infoCallback(d, bestValue, elapsedMs(), nodeCount, pv);
        }

        // Early termination: đã tìm được mate → không cần tìm thêm
        if (bestValue >= ScoreMate - MaxPly || bestValue <= -ScoreMate + MaxPly) {
            break;
        }
    }

    bestScore = bestValue;
    rootPv = pv;

    if (countBestMove <= 0) {
        // Fallback: không tìm được nước nào (search abort quá sớm) → trả nước đầu tiên hợp lệ
        auto movePtr = buff[0];
        int count = MoveGen::instance->genMoveList(board, movePtr);
        return count > 0 ? movePtr[0] : Move();
    }

    int r = getRand(0, countBestMove - 1);
    return bestMoves[r];
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
int Engine::negamax(Board &board, int depth, int alpha, int beta, PVLine &pv, bool allowNull){
    pv.count = 0;

    // Kiểm tra thời gian định kỳ (mỗi CheckTimeInterval nodes)
    nodeCount++;
    if (!isRoot && useTimeControl && (nodeCount & (CheckTimeInterval - 1)) == 0) {
        if (shouldStop()) {
            searchAborted = true;
            return 0;  // Giá trị trả về không quan trọng, sẽ bị bỏ qua
        }
    }
    if (searchAborted) return 0;

    bool inCheck = board.anyCheck();

    // Điều kiện dừng: hết độ sâu tìm kiếm
    // Check extension: khi bị chiếu ở depth=0, cho thêm 1 ply để tìm nước thoát
    // depth < 0: luôn vào quiescence (tránh buff[negative] = undefined behavior)
    if (depth <= 0) {
        if (!inCheck || depth < 0) {
            return quiescence<color>(board, alpha, beta);
        }
        // Chỉ tới đây khi depth==0 && inCheck → check extension 1 ply
    }

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

    // === Null Move Pruning ===
    if (!isRoot && allowNull && !inCheck && depth >= (1 + NULL_MOVE_R)) {
        BB myPieces = board.colors[color];
        BB bigPieces = myPieces & (board.types[Knight] | board.types[Bishop]
                                  | board.types[Rook] | board.types[Queen]);
        if (bigPieces) {
            BoardState nullState;
            board.doNullMove(nullState);
            PVLine nullPv;
            int nullScore = -negamax<!color, false>(board, depth - 1 - NULL_MOVE_R, -beta, -beta + 1, nullPv, false);
            board.undoNullMove();

            if (nullScore >= beta && nullScore > -ScoreMate + MaxPly && nullScore < ScoreMate - MaxPly) {
                return nullScore;
            }
        }
    }

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
        if (m.raw() == ttMoveRaw && ttMoveRaw != 0) { m.val = 30000; continue; }
        if (m.is<Move::Capture>()) {
            // MVV-LVA: victim value * 10 - attacker value
            PieceType capturedType;
            if (m.is<Move::Enpassant>()) {
                capturedType = Pawn;
            } else {
                Piece captured = board.pieces[m.dst()];
                capturedType = (captured != PieceNone) ? pieceToType(captured) : PieceTypeNone;
            }
            PieceType attackerType = pieceToType(board.pieces[m.src()]);
            m.val = 10000 + mvvlva[capturedType] * 10 - mvvlva[attackerType];
        } else if (m.is<Move::Promotion>()) {
            // Non-capture promotion (queen promotion rất mạnh)
            m.val = 9500;
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
    PVLine childPv;

    // Duyệt qua tất cả nước đi (pick-best: chỉ đưa nước tốt nhất lên, không sort hết)
    for (int i = 0; i < count; i++){
        // Tìm nước có val cao nhất từ [i..count) và swap lên vị trí i
        for (int j = i + 1; j < count; j++) {
            if (movePtr[j].val > movePtr[i].val)
                std::swap(movePtr[i], movePtr[j]);
        }
        auto move = movePtr[i];

        // Pre-compute SEE cho capture moves (PHẢI gọi TRƯỚC doMove,
        // vì SEE đọc board.pieces[src] để xác định quân tấn công)
        int seeValue = 0;
        if (move.is<Move::Capture>() && !move.is<Move::Promotion>()) {
            seeValue = see(board, move);
        }

        board.doMove(move, state);

        int score;
        // PVS: nước đầu tiên tìm full window, các nước sau tìm zero-window trước
        // Không áp dụng PVS ở root (cần score chính xác cho mọi nước)
        if (isRoot || i == 0) {
            score = -negamax<!color, false>(board, depth - 1, -beta, -alpha, childPv);
        } else {
            int newDepth = depth - 1;
            // LMR: giảm depth cho nước quiet muộn
            if (i >= LMR_FULL_MOVES && depth >= LMR_MIN_DEPTH && !inCheck
                && !move.is<Move::Capture>() && !move.is<Move::Promotion>()
                && move.raw() != killer0 && move.raw() != killer1) {
                newDepth -= 1;
            }
            // LMR cho captures thua lỗ (SEE < 0): cũng reduce depth
            else if (i >= LMR_FULL_MOVES && depth >= LMR_MIN_DEPTH && !inCheck
                && move.is<Move::Capture>() && !move.is<Move::Promotion>()
                && seeValue < 0) {
                newDepth -= 1;
            }
            // Zero-window search
            score = -negamax<!color, false>(board, newDepth, -alpha - 1, -alpha, childPv);
            // Re-search full window nếu fail high
            if (score > alpha && score < beta) {
                score = -negamax<!color, false>(board, depth - 1, -beta, -alpha, childPv);
            }
        }
        move.val = score;
        board.undoMove(move);

        // Nếu search bị abort → thoát ngay
        if (searchAborted) return 0;

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

            // Cập nhật PV khi cải thiện alpha
            if (score > origAlpha) {
                pv.update(move, childPv);
            }

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

    return bestValue;

}

/**
 * @brief Quiescence Search - tìm kiếm thêm chỉ các nước bắt quân
 * cho đến khi vị thế "yên tĩnh" (không còn bắt quân có lợi)
 * Giải quyết horizon effect: tránh đánh giá sai khi đang giữa chuỗi đổi quân
 */
template <Color color>
int Engine::quiescence(Board &board, int alpha, int beta, int qdepth) {
    // Kiểm tra abort
    nodeCount++;
    if (useTimeControl && (nodeCount & (CheckTimeInterval - 1)) == 0) {
        if (shouldStop()) {
            searchAborted = true;
            return 0;
        }
    }
    if (searchAborted) return 0;

    // Đánh giá tĩnh (stand-pat): chỉ material + PST, cần nhanh vì gọi ở mọi leaf node
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
        if (movePtr[i].is<Move::Enpassant>()) {
            movePtr[i].val = 100; // En passant bắt tốt = 100
        } else {
            Piece captured = board.pieces[movePtr[i].dst()];
            movePtr[i].val = (captured != PieceNone) ? pieceValues[captured] : 0;
        }
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
