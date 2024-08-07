#pragma once
#ifndef __linux__
#include <intrin.h>
#endif
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

constexpr u64 pow2(int v){
    return v == 0 ? 1 : pow2(v - 1) * 2;
}

constexpr int Infinity = 1000000000;

enum Color: u8 {
    White, Black, Color_NB
};
// Create toggle operator for color
constexpr Color operator!(Color c){
    return Color(!int(c));
}
enum PieceType : u8 {
    PieceTypeNone = 0,
    Pawn, Bishop, Knight, Rook, Queen, King, PieceType_NB = 8
};
enum Piece : u8 {
    PieceNone = 0,
    WhitePawn, WhiteBishop, WhiteKnight, WhiteRook, WhiteQueen, WhiteKing,
    BlackPawn = WhitePawn + 8, BlackBishop, BlackKnight, BlackRook, BlackQueen, BlackKing,
    Piece_NB = 16
};
constexpr PieceType pieceToType(Piece piece){
    return PieceType(piece % PieceType_NB);
}
constexpr Color pieceToColor(Piece piece){
    return Color(piece / PieceType_NB);
}
constexpr Piece makePiece(Color color, PieceType type){
    return Piece(color * PieceType_NB + type);
}
char pieceToChar(Piece piece);
Piece charToPiece(char c);
enum Rank : u8 {
    Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank_NB
};
char rankToChar(Rank r);
Rank charToRank(char c);
enum File : u8 {
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, File_NB
};
char fileToChar(File f);
File charToFile(char c);
enum Square: u8 {
    _A1, _B1, _C1, _D1, _E1, _F1, _G1, _H1,
    _A2, _B2, _C2, _D2, _E2, _F2, _G2, _H2,
    _A3, _B3, _C3, _D3, _E3, _F3, _G3, _H3,
    _A4, _B4, _C4, _D4, _E4, _F4, _G4, _H4,
    _A5, _B5, _C5, _D5, _E5, _F5, _G5, _H5,
    _A6, _B6, _C6, _D6, _E6, _F6, _G6, _H6,
    _A7, _B7, _C7, _D7, _E7, _F7, _G7, _H7,
    _A8, _B8, _C8, _D8, _E8, _F8, _G8, _H8,
    SquareNone = 0,
    Square_Count = 64
};
constexpr static Square makeSquare(Rank r, File f){
    return Square(r * 8 + f);
}
constexpr static Rank getRank(Square square){
    return Rank(square / 8);
}
constexpr static File getFile(Square square){
    return File(square % 8);
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

using BB = u64;

constexpr BB Rank1_BB = 0xffULL;
constexpr BB FileA_BB = 0x0101010101010101ULL;

constexpr static BB rankBB(Rank r){
    return Rank1_BB << (r * 8);
}

constexpr static BB fileBB(File f){
    return FileA_BB << f;
}

constexpr BB All_BB = 0xffffffffffffffffULL;

constexpr static BB squareToBB(Square sq){
    return 1ULL << sq;
}
constexpr BB squareToBB(int idx){
    return 1ULL << idx;
}

inline int bbToSquare(u64 bb) {
    unsigned long idx;
#ifdef __linux__
    idx = __builtin_ctzll(bb);
#else
    _BitScanForward64(&idx, bb);
#endif
    return (Square) idx;
}
inline int popCount(u64 bb){
#ifdef __linux__
    return __builtin_popcountll(bb);
#else
    return __popcnt64(bb);
#endif
}

// Vị trí của quân nhập thành
constexpr static Square CastlingWKOO = _G1;
constexpr static BB CastlingWROO_Toggle = squareToBB(_F1) | squareToBB(_H1);
constexpr static Square CastlingWKOOO = _C1;
constexpr static BB CastlingWROOO_Toggle = squareToBB(_A1) | squareToBB(_D1);

constexpr static Square CastlingBKOO = _G8;
constexpr static BB CastlingBROO_Toggle = squareToBB(_F8) | squareToBB(_H8);
constexpr static Square CastlingBKOOO = _C8;
constexpr static BB CastlingBROOO_Toggle = squareToBB(_A8) | squareToBB(_D8);

constexpr static BB CastlingWOOSpace = squareToBB(_F1) | squareToBB(_G1);
constexpr static BB CastlingWOOOSpace = squareToBB(_B1) | squareToBB(_C1) | squareToBB(_D1);

constexpr static BB CastlingBOOSpace = squareToBB(_F8) | squareToBB(_G8);
constexpr static BB CastlingBOOOSpace = squareToBB(_B8) | squareToBB(_C8) | squareToBB(_D8);


class BoardState {
    u16 state;
public:
    constexpr BoardState() : state(0) {}
    constexpr BoardState(u8 initialState) : state(initialState) {}

    // Get active
    constexpr inline Color getColorActive() const { return Color(state & 0x01); }
    constexpr inline bool getActive() const { return state & 0x01; }

    // Set active
    constexpr inline void setActive(Color c) { state = (state & 0xfe) | c; }
    constexpr inline void setToggleActive() { state ^= 1; }

    // Castling
    constexpr inline bool getWhiteOO() const { return state & 0x02; }
    constexpr inline bool getWhiteOOO() const { return state & 0x04; }
    constexpr inline bool getBlackOO() const { return state & 0x08; }
    constexpr inline bool getBlackOOO() const { return state & 0x10; }

    constexpr inline void setWhiteOO(bool v) { state = (state & 0xfffd) | (u16(v) << 1); }
    constexpr inline void setWhiteOOO(bool v) { state = (state & 0xfffb) | (u16(v) << 2); }
    constexpr inline void setBlackOO(bool v) { state = (state & 0xfff7) | (u16(v) << 3); }
    constexpr inline void setBlackOOO(bool v) { state = (state & 0xffef) | (u16(v) << 4); }

    // Enpassant
    constexpr inline bool getEnpassant() const { return state & 0x20; }


};

class Move {
public:
    enum MoveValue {
        NullMove = 0
    };

    enum Type : u16 {
        Normal,
        Enpassant = (1 << 14),
        Castling = (2 << 14),
        Promotion = (3 << 14),
    };

    constexpr inline Move() : move(NullMove){}
    constexpr inline Move(u16 m) : move(m) {}

    constexpr inline bool operator!=(MoveValue val){
        return move != val;
    }

    constexpr inline int src() const { return move & 0x3f; }
    constexpr inline int dst() const { return (move >> 6) & 0x3f; }
    constexpr inline int getPiecePromotion() const { return PieceType(((move >> 12) & 0x03) + 1); }
    constexpr inline int type() const { return move & (3 << 14); }

    std::string getDescription() const;

    static Move makeNormalMove(int src, int dst) { return Move(src + (dst << 6)); }
    static Move makeEnpassantMove(int src, int dst) { return Move(src + (dst << 6) + Enpassant); }
    static Move makeCastlingMove(int src, int dst) { return Move(src + (dst << 6) + Castling); }
    static Move makePromotionMove(int src, int dst, PieceType p) { return Move(src + (dst << 6) + Promotion + ((p - 1) << 12)); }
private:
    u16 move;
};
}
















