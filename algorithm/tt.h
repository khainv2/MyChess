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
    u8  age;        // Để replacement policy
};

class TranspositionTable {
public:
    TranspositionTable();
    ~TranspositionTable();

    void resize(size_t sizeMB);
    void clear();

    void store(u64 key, int score, int depth, TTFlag flag, Move bestMove);
    bool probe(u64 key, TTEntry &entry) const;

    size_t getSize() const { return numEntries; }

private:
    TTEntry *table = nullptr;
    size_t numEntries = 0;
};

} // namespace kc
