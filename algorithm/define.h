#pragma once
#ifndef __linux__
#include <intrin.h>
#endif
#include <string>

#ifdef C2
#undef C2
#endif

namespace kc {
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
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
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

std::string bbToString(BB bb);
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
constexpr static Square CastlingWKOO = G1;
constexpr static BB CastlingWROO_Toggle = squareToBB(F1) | squareToBB(H1);
constexpr static Square CastlingWKOOO = C1;
constexpr static BB CastlingWROOO_Toggle = squareToBB(A1) | squareToBB(D1);

constexpr static Square CastlingBKOO = G8;
constexpr static BB CastlingBROO_Toggle = squareToBB(F8) | squareToBB(H8);
constexpr static Square CastlingBKOOO = C8;
constexpr static BB CastlingBROOO_Toggle = squareToBB(A8) | squareToBB(D8);

constexpr static BB CastlingWOOSpace = squareToBB(F1) | squareToBB(G1);
constexpr static BB CastlingWOOOSpace = squareToBB(B1) | squareToBB(C1) | squareToBB(D1);

constexpr static BB CastlingBOOSpace = squareToBB(F8) | squareToBB(G8);
constexpr static BB CastlingBOOOSpace = squareToBB(B8) | squareToBB(C8) | squareToBB(D8);


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
    constexpr inline PieceType getPromotionPieceType() const { return PieceType(((move >> 12) & 0x03) + 2); }
    constexpr inline int type() const { return move & (3 << 14); }

    std::string getDescription() const;

    static constexpr inline Move makeNormalMove(int src, int dst) { return Move(src + (dst << 6)); }
    static constexpr inline Move makeEnpassantMove(int src, int dst) { return Move(src + (dst << 6) + Enpassant); }
    static constexpr inline Move makeCastlingMove(int src, int dst) { return Move(src + (dst << 6) + Castling); }
    static constexpr inline Move makePromotionMove(int src, int dst, PieceType p) { return Move(src + (dst << 6) + Promotion + ((p - 2) << 12)); }
private:
    u16 move;
};
}
















