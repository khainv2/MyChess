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
    currentAge = 0;
    stats = TTStats();
}

void TranspositionTable::newSearch() {
    // Wrap-around an toàn vì entryWorth() dùng (u8) subtraction.
    currentAge++;
}

size_t TranspositionTable::countCurrentAgeEntries() const {
    size_t n = 0;
    for (size_t i = 0; i < numEntries; i++) {
        // key != 0 coi như slot có dữ liệu (xác suất key = 0 thật sự ~ 1/2^64)
        if (table[i].key != 0 && table[i].age == currentAge) n++;
    }
    return n;
}

void TranspositionTable::store(u64 key, int score, int depth, TTFlag flag, Move bestMove) {
    stats.stores++;

    size_t idx = key % numEntries;
    TTEntry &e = table[idx];

    // Trường hợp 1: slot trống
    if (e.key == 0) {
        e.key      = key;
        e.score    = static_cast<i16>(score);
        e.depth    = static_cast<i16>(depth);
        e.flag     = static_cast<u8>(flag);
        e.bestMove = bestMove.raw();
        e.age      = currentAge;
        stats.writes++;
        stats.emptyWrites++;
        return;
    }

    // Trường hợp 2: cùng vị trí — refresh lại
    if (e.key == key) {
        // Chỉ ghi đè score/depth nếu depth mới >= cũ hoặc entry là EXACT
        // (PV node đáng giữ hơn bound node cùng depth)
        if (depth >= e.depth || flag == TT_EXACT) {
            e.score = static_cast<i16>(score);
            e.depth = static_cast<i16>(depth);
            e.flag  = static_cast<u8>(flag);
        }
        // Best move luôn cập nhật nếu có (move mới thường đáng tin hơn)
        if (bestMove.raw() != 0) e.bestMove = bestMove.raw();
        e.age = currentAge;  // refresh age để entry này không bị evict oan
        stats.writes++;
        stats.sameKeyWrites++;
        return;
    }

    // Trường hợp 3: collision — dùng công thức worth để quyết định
    // Entry mới có "worth ảo" = depth (ageDiff = 0 vì age = currentAge)
    int oldWorth = entryWorth(e);
    bool oldAgeStale = (e.age != currentAge);

    if (depth >= oldWorth) {
        // Ghi đè
        if (oldAgeStale) stats.ageEvicts++;
        else             stats.depthEvicts++;

        e.key      = key;
        e.score    = static_cast<i16>(score);
        e.depth    = static_cast<i16>(depth);
        e.flag     = static_cast<u8>(flag);
        e.bestMove = bestMove.raw();
        e.age      = currentAge;
        stats.writes++;
    } else {
        stats.rejected++;
    }
}

bool TranspositionTable::probe(u64 key, TTEntry &entry) {
    stats.probes++;
    size_t idx = key % numEntries;
    const TTEntry &e = table[idx];
    if (e.key == key) {
        entry = e;
        stats.hits++;
        return true;
    }
    return false;
}
