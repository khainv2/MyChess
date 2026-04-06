#include "tt.h"
#include "board.h"

using namespace kc;

// ==================== Zobrist Hashing ====================

u64 zobrist::pieces[16][64];
u64 zobrist::side;
u64 zobrist::castling[16];
u64 zobrist::enPassant[64];

void zobrist::init() {
    std::mt19937_64 rng(0xBEEF1234DEAD5678ULL);

    for (int p = 0; p < 16; p++)
        for (int sq = 0; sq < 64; sq++)
            pieces[p][sq] = rng();

    side = rng();

    for (int i = 0; i < 16; i++)
        castling[i] = rng();

    for (int sq = 0; sq < 64; sq++)
        enPassant[sq] = rng();
}

u64 kc::computeHash(const Board &board) {
    u64 h = 0;
    for (int sq = 0; sq < 64; sq++) {
        Piece pc = board.pieces[sq];
        if (pc != PieceNone)
            h ^= zobrist::pieces[pc][sq];
    }
    if (board.side == Black)
        h ^= zobrist::side;
    if (board.state) {
        h ^= zobrist::castling[board.state->castlingRights & 0xF];
        if (board.state->enPassant != SquareNone)
            h ^= zobrist::enPassant[board.state->enPassant];
    }
    return h;
}

// ==================== Transposition Table ====================

TranspositionTable::TranspositionTable() {
    resize(32); // 32MB mặc định
}

TranspositionTable::~TranspositionTable() {
    delete[] table;
}

void TranspositionTable::resize(size_t sizeMB) {
    delete[] table;
    numEntries = (sizeMB * 1024 * 1024) / sizeof(TTEntry);
    if (numEntries == 0) numEntries = 1;
    table = new TTEntry[numEntries];
    clear();
}

void TranspositionTable::clear() {
    std::memset(table, 0, numEntries * sizeof(TTEntry));
}

void TranspositionTable::store(u64 key, int score, int depth, TTFlag flag, Move bestMove) {
    size_t idx = key % numEntries;
    TTEntry &e = table[idx];

    // Replace nếu: entry trống, depth mới >= depth cũ, hoặc key trùng VÀ depth mới >= depth cũ
    if (e.key == 0 || depth >= e.depth) {
        e.key = key;
        e.score = static_cast<i16>(score);
        e.depth = static_cast<i16>(depth);
        e.flag = static_cast<u8>(flag);
        e.bestMove = bestMove.raw();
    } else if (e.key == key && bestMove.raw() != 0) {
        // Cùng position nhưng depth thấp hơn: chỉ cập nhật best move (không ghi đè score/depth)
        e.bestMove = bestMove.raw();
    }
}

bool TranspositionTable::probe(u64 key, TTEntry &entry) const {
    size_t idx = key % numEntries;
    const TTEntry &e = table[idx];
    if (e.key == key) {
        entry = e;
        return true;
    }
    return false;
}
