#pragma once
#include "board.h"
#include "define.h"
#include "tt.h"
#include <chrono>
#include <cstring>
#include <functional>
#include <atomic>

namespace kc {

/**
 * @brief Cấu trúc thông tin thời gian cho một lần tính toán
 * Hỗ trợ 3 chế độ: fixed depth, move time, hoặc time control (remaining + increment)
 */
/**
 * @brief Principal Variation - chuỗi nước đi tốt nhất từ search
 */
struct PVLine {
    int count = 0;
    Move moves[64];

    void update(Move move, const PVLine &child) {
        moves[0] = move;
        count = child.count + 1;
        if (child.count > 0)
            std::memcpy(moves + 1, child.moves, child.count * sizeof(Move));
    }
};

struct TimeInfo {
    int depth = 0;          // Fixed depth (0 = không giới hạn depth, dùng thời gian)
    int moveTimeMs = 0;     // Thời gian cố định cho nước đi này (ms), 0 = không dùng
    int remainingMs = 0;    // Thời gian còn lại (ms)
    int incrementMs = 0;    // Thời gian cộng thêm mỗi nước (ms)
    int movesToGo = 0;      // Số nước còn lại đến time control tiếp (0 = sudden death)
};

class Engine {
    enum {
        MaxPly = 32,
        MaxDepthLimit = 64,         // Giới hạn depth tối đa khi dùng time control
        CheckTimeInterval = 2048,   // Kiểm tra thời gian mỗi N nodes
    };
public:
    Engine();
    Move calc(const Board &chessBoard);
    Move calc(const Board &chessBoard, const TimeInfo &timeInfo);

    int getCompletedDepth() const { return completedDepth; }
    int getBestScore() const { return bestScore; }
    const PVLine& getPV() const { return rootPv; }
    u64 getNodeCount() const { return nodeCount; }
    void abort() { searchAborted = true; }

    // Callback gọi sau mỗi iteration hoàn chỉnh (cho UCI info output)
    using InfoCallback = std::function<void(int depth, int score, int timeMs, u64 nodes, const PVLine &pv)>;
    void setInfoCallback(InfoCallback cb) { infoCallback = std::move(cb); }

    template <Color color, bool isRoot>
    int negamax(Board &board, int depth, int alpha, int beta, PVLine &pv, bool allowNull = true);

    template <Color color>
    int quiescence(Board &board, int alpha, int beta, int qdepth = 0);

private:
    static constexpr int MaxQDepth = 32;
    Move bestMoves[256];
    int countBestMove = 0;
    int fixedDepth;

    Move bestMoveHistories[256][MaxPly];
    int bestMoveHistoryCount[256] = { 0 };
    Move moves[MaxPly];
    int movePlyCount = 0;
    Move buff[MaxPly * 2][256];
    Move qbuff[MaxQDepth][64];

    // Killer moves: 2 killers per depth
    static constexpr int NumKillers = 2;
    Move killers[MaxPly][NumKillers];

    // History heuristic: quiet move gây cutoff bao nhiêu lần [piece][toSquare]
    int history[Piece_NB][Square_NB];

    TranspositionTable tt;

    int completedDepth = 0;     // Depth hoàn chỉnh cuối cùng (để báo cáo)
    int bestScore = 0;          // Score của best move (để báo cáo mate-in-N)
    PVLine rootPv;              // Principal Variation từ iteration hoàn chỉnh cuối

    // === Time Management ===
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint searchStart;      // Thời điểm bắt đầu tìm kiếm
    int softLimitMs = 0;        // Soft limit: kết thúc iteration hiện tại nếu vượt
    int hardLimitMs = 0;        // Hard limit: abort search ngay lập tức
    bool useTimeControl = false;        // Có đang dùng time control không
    std::atomic<bool> searchAborted{false}; // Flag báo search bị abort (atomic cho cross-thread stop)
    u64 nodeCount = 0;                  // Đếm số nodes đã duyệt
    InfoCallback infoCallback;

    void allocateTime(const TimeInfo &timeInfo);
    bool shouldStop();          // Kiểm tra có nên dừng search không
    int elapsedMs() const;      // Thời gian đã trôi qua (ms)
};
}
