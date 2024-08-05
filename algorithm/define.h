#pragma once
#include <intrin.h>
#include <string>

#ifdef _C2
#undef _C2
#endif

namespace kchess {
using u8 = unsigned char;
using u16 = unsigned int;
using u32 = unsigned int;
using i64 = long long;
using u64 = unsigned long long;

constexpr int Infinity = 1000000000;

enum Color: u8 {
    White, Black, Color_NB
};
constexpr Color toggleColor(Color c){
    return Color(c ^ 1);
}
enum Piece : u8 {
    Pawn, Bishop, Knight, Rook, Queen, King, Piece_NB
};
enum Rank : u8 {
    Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank_NB
};
enum File : u8 {
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, File_NB
};
enum Square: u8 {
    _A1, _B1, _C1, _D1, _E1, _F1, _G1, _H1,
    _A2, _B2, _C2, _D2, _E2, _F2, _G2, _H2,
    _A3, _B3, _C3, _D3, _E3, _F3, _G3, _H3,
    _A4, _B4, _C4, _D4, _E4, _F4, _G4, _H4,
    _A5, _B5, _C5, _D5, _E5, _F5, _G5, _H5,
    _A6, _B6, _C6, _D6, _E6, _F6, _G6, _H6,
    _A7, _B7, _C7, _D7, _E7, _F7, _G7, _H7,
    _A8, _B8, _C8, _D8, _E8, _F8, _G8, _H8,
    Square_NB = 64
};

constexpr Square makeSquare(Rank r, File f){
    return Square(r * 8 + f);
}
enum Value {
    Value_Pawn = 100,
    Value_Bishop = 300,
    Value_Knight = 300,
    Value_Rook = 500,
    Value_Queen = 900,
    Value_King = 32000,
};

enum : u64 {
    B_D  = 0x00000000000000ffULL,
    B_U  = 0xff00000000000000ULL,
    B_L  = 0x0101010101010101ULL,
    B_R  = 0x8080808080808080ULL,

    B_D2 = 0x000000000000ffffULL,
    B_U2 = 0xffff000000000000ULL,
    B_L2 = 0x0303030303030303ULL,
    B_R2 = 0xC0C0C0C0C0C0C0C0ULL,
};

constexpr u64 lsbBB(u64 input) { return input & (-i64(input)); }

using Bitboard = u64;
using BoardState = u8;

constexpr Bitboard Rank1_BB = 0xffULL;
constexpr Bitboard FileA_BB = 0x0101010101010101ULL;

constexpr Bitboard All_BB = 0xffffffffffffffffULL;

constexpr Bitboard squareToBB(Square sq){
    return 1ULL << sq;
}
constexpr Bitboard squareToBB(int idx){
    return 1ULL << idx;
}

inline int bbToSquare(u64 bb) {
    unsigned long idx;
    _BitScanForward64(&idx, bb);
    return (Square) idx;
}
inline int popCount(u64 bb){
    return __popcnt64(bb);
}

constexpr static Bitboard CastlingWhiteQueenSideSpace = squareToBB(_B1) | squareToBB(_C1) | squareToBB(_D1);
constexpr static Bitboard CastlingWhiteKingSideSpace = squareToBB(_F1) | squareToBB(_G1);

constexpr static Bitboard CastlingBlackQueenSideSpace = squareToBB(_B8) | squareToBB(_C8) | squareToBB(_D8);
constexpr static Bitboard CastlingBlackKingSideSpace = squareToBB(_F8) | squareToBB(_G8);

// 1 bit active color, 4 bit castling,
// 1 bit is enpassant, 3 bit enpasant position
// 4 bit count half move (for fifty-move rule)

// Get active
constexpr Color getColorActive(BoardState state) { return Color(state & 0x01); }
constexpr bool getActive(BoardState state){ return state & 0x01; }

// Castling
constexpr bool getWhiteOO(BoardState state){ return state & 0x02; }
constexpr bool getWhiteOOO(BoardState state){ return state & 0x04; }
constexpr bool getBlackOO(BoardState state){ return state & 0x04; }
constexpr bool getBlackOOO(BoardState state){ return state & 0x08; }

constexpr u64 pow2(int v){
    return v == 0 ? 1 : pow2(v - 1) * 2;
}

// 5 bit source, 5 bit destination, 2 bit move
using Move = u16;
enum MoveType {
    Normal,
    Enpassant = (1 << 14),
    Castling = (2 << 14),
    Promotion = (3 << 14),
};
constexpr int getSourceFromMove(Move move){ return move & 0x3f; }
constexpr int getDestinationFromMove(Move move){ return (move >> 6) & 0x3f; }
constexpr int getPiecePromotion(Move move){ return Piece(((move >> 12) & 0x03) + 1); }
constexpr int getMoveType(Move move){ return move & (3 << 14); }
std::string getMoveDescription(Move move);

constexpr int makeMove(int src, int dst){ return src + (dst << 6); }
constexpr int makeMoveEnpassant(int src, int dst){ return src + (dst << 6) + Enpassant; }
constexpr int makeMoveCastling(int src, int dst){ return src + (dst << 6) + Castling; }
constexpr int makeMovePromotion(int src, int dst, Piece p){ return src + (dst << 6) + Promotion + ((p - 1) << 12); }


}




















