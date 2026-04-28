#pragma once
#include "define.h"
#include <cstring>
#include <random>

namespace kc {

// ==================== Zobrist Hashing ====================
namespace zobrist {

extern u64 pieces[16][64];    // [Piece][Square]
extern u64 side;              // XOR khi đổi bên
extern u64 castling[16];      // [CastlingRights combination]
extern u64 enPassant[64];     // [Square]

void init();

} // namespace zobrist

// Tính hash cho một Board
u64 computeHash(const struct Board &board);

// ==================== Transposition Table ====================

enum TTFlag : u8 {
    TT_EXACT = 0,       // Giá trị chính xác (PV node)
    TT_LOWER_BOUND = 1, // Score >= beta (cut node)
    TT_UPPER_BOUND = 2, // Score <= alpha (all node)
};

struct TTEntry {
    u64 key;        // Zobrist hash để xác minh
    i16 score;      // Điểm đánh giá
    i16 depth;      // Độ sâu đã tìm
    u16 bestMove;   // Best move (packed: src6 + dst6 + flag4)
    u8  flag;       // TTFlag
    u8  age;        // Generation counter, tăng mỗi newSearch()
};

// Thống kê phục vụ instrumentation/benchmark, reset qua resetStats()
struct TTStats {
    u64 probes       = 0;  // Số lần probe()
    u64 hits         = 0;  // Số lần probe() trả true (key khớp)
    u64 stores       = 0;  // Số lần store() được gọi
    u64 writes       = 0;  // Số lần thật sự ghi vào bảng
    u64 sameKeyWrites= 0;  // writes với entry cùng key (refresh)
    u64 emptyWrites  = 0;  // writes vào slot trống
    u64 ageEvicts    = 0;  // writes thay thế entry age cũ (khác key)
    u64 depthEvicts  = 0;  // writes thay thế entry cùng age (khác key)
    u64 rejected     = 0;  // store() không ghi (giữ entry cũ)
};

class TranspositionTable {
public:
    TranspositionTable();
    ~TranspositionTable();

    void resize(size_t sizeMB);
    void clear();

    // Bắt đầu một lượt search mới: tăng generation counter.
    // KHÔNG xoá bảng — giữ nguyên entry cũ để reuse giữa các calc().
    void newSearch();

    void store(u64 key, int score, int depth, TTFlag flag, Move bestMove);
    bool probe(u64 key, TTEntry &entry);

    size_t getSize() const { return numEntries; }
    u8 getAge() const { return currentAge; }

    // Instrumentation
    const TTStats& getStats() const { return stats; }
    void resetStats() { stats = TTStats(); }

    // Đếm số entry có age == currentAge trong toàn bảng (chậm, chỉ dùng debug)
    size_t countCurrentAgeEntries() const;

private:
    TTEntry *table = nullptr;
    size_t numEntries = 0;
    u8 currentAge = 0;
    mutable TTStats stats;

    // "Worth" của một entry: càng cao càng đáng giữ.
    // Entry age cũ bị phạt 4 ply depth mỗi generation lùi.
    int entryWorth(const TTEntry &e) const {
        int ageDiff = (int)(u8)(currentAge - e.age);  // wrap-around an toàn
        return (int)e.depth - 4 * ageDiff;
    }
};

} // namespace kc
