#pragma once
#include <intrin.h>

namespace kchess {
using u8 = unsigned char;
using u16 = unsigned int;
using u32 = unsigned int;
using i64 = long long;
using u64 = unsigned long long;

enum Color: u8 {
    White, Black, Color_NB
};
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
    Square_NB = 64
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

constexpr u64 Rank1_BB = 0xffULL;
constexpr u64 FileA_BB = 0x0101010101010101ULL;

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
//inline Square lsb

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

// Enpassant avaiable?
//constexpr bool

struct ChessBoard {
    Bitboard whites = 0;
    Bitboard blacks = 0;

    Bitboard pawns = 0;
    Bitboard bishops = 0;
    Bitboard knights = 0;
    Bitboard rooks = 0;
    Bitboard queens = 0;
    Bitboard kings = 0;

    BoardState state = 0;

    ChessBoard(){
    }

    Bitboard getBitBoard(Piece p, Color c) const {
        auto bb = reinterpret_cast<const Bitboard *>(this);
        return bb[c] & bb[p + 2];
    }

    constexpr Bitboard all() const { return blacks | whites; }
    constexpr Bitboard mines() const { return getColorActive(state) == White ? whites : blacks; }
    constexpr Bitboard enemies() const { return getColorActive(state) == White ? blacks : whites; }
};

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

constexpr int makeMove(int src, int dst){ return src + (dst << 6); }
constexpr int makeMoveEnpassant(int src, int dst){ return src + (dst << 6) + Enpassant; }
constexpr int makeMoveCastling(int src, int dst){ return src + (dst << 6) + Castling; }
constexpr int makeMovePromotion(int src, int dst, Piece p){ return src + (dst << 6) + Promotion + ((p - 1) << 12); }




}




















