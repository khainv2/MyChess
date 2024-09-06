#pragma once
#ifndef __linux__
#include <intrin.h>
#endif
#include <string>

namespace kc {
using u8 = unsigned char;
using i16 = short;
using u16 = unsigned short;
using u32 = unsigned int;
using i64 = long long;
using u64 = unsigned long long;

#define Bitloop(X) for(;X; X = _blsr_u64(X))

constexpr u64 pow2(int v) noexcept {
    return v == 0 ? 1 : pow2(v - 1) * 2;
}

template <int v>
u64 pow2() noexcept {
    if constexpr (v == 0){
        return 1;
    } else {
        return pow2(v - 1) * 2;
    }
}

constexpr int Infinity = 32002;

enum Color: u8 {
    White, Black, Color_NB
};
// Create toggle operator for color
static constexpr Color operator!(Color c){
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
constexpr PieceType pieceToType(Piece piece) noexcept {
    return PieceType(piece % PieceType_NB);
}
constexpr Color pieceToColor(Piece piece) noexcept {
    return Color(piece / PieceType_NB);
}
constexpr Piece makePiece(Color color, PieceType type) noexcept {
    return Piece(color * PieceType_NB + type);
}
template <Color color, PieceType type>
constexpr Piece makePiece() noexcept {
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
enum Square: int {
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
constexpr static Square makeSquare(Rank r, File f) noexcept {
    return Square((r << 3) + f);
}
constexpr static Rank getRank(Square square) noexcept {
    return Rank(square >> 3);
}
constexpr static File getFile(Square square) noexcept {
    return File(square & 7);
}

constexpr static Rank getRank(int square) noexcept {
    return Rank(square >> 3);
}
constexpr static File getFile(int square) noexcept {
    return File(square & 7);
}

enum Direction : int {
    North = 8,
    East  = 1,
    South = -North,
    West  = -East,

    NorthEast = North + East,
    SouthEast = South + East,
    SouthWest = South + West,
    NorthWest = North + West
};

enum Value {
    Value_Pawn = 208,
    Value_Bishop = 825,
    Value_Knight = 781,
    Value_Rook = 1276,
    Value_Queen = 2538,
//    Value_King = 32000,
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

constexpr u64 lsbBB (u64 input) noexcept  { return input & (-i64(input)); }

using BB = u64;


std::string bbToString(BB bb);
constexpr BB Rank1_BB = 0xffULL;
constexpr BB FileA_BB = 0x0101010101010101ULL;
constexpr BB LeftDiag_BB = 0x8040201008040201ULL;
constexpr BB RightDiag_BB = 0x102040810204080ULL;

constexpr static BB rankBB(Rank r) noexcept {
    return Rank1_BB << (r << 3);
}
template <Rank r>
constexpr static BB rankBB() noexcept {
    return Rank1_BB << (r << 3);
}

constexpr static BB fileBB(File f) noexcept {
    return FileA_BB << f;
}

constexpr static inline int leftDiagOffset(int index) noexcept {
    return getFile(index) - getRank(index);
}
constexpr static inline int rightDiagOffset(int index) noexcept {
    return 7 - getFile(index) - getRank(index);
}
constexpr static inline BB getLeftDiag(int index) noexcept {
    int offset = leftDiagOffset(index) << 3;
    if (offset > 0){
        return LeftDiag_BB >> offset;
    } else {
        return LeftDiag_BB << -offset;
    }
}
constexpr static inline BB getRightDiag(int index) noexcept {
    int offset = rightDiagOffset(index) << 3;
    if (offset > 0){
        return RightDiag_BB >> offset;
    } else {
        return RightDiag_BB << -offset;
    }
}
template <Color color>
constexpr static BB getSideDiag(int index) noexcept {
    if constexpr (color == White) {
        return getRightDiag(index);
    } else {
        return getLeftDiag(index);
    }
}

constexpr BB All_BB = 0xffffffffffffffffULL;


template<Direction D>
constexpr BB shift(BB b) {
    return D == North         ? b << 8
         : D == South         ? b >> 8
         : D == North + North ? b << 16
         : D == South + South ? b >> 16
         : D == East          ? (b & ~fileBB(FileH)) << 1
         : D == West          ? (b & ~fileBB(FileA)) >> 1
         : D == NorthEast    ? (b & ~fileBB(FileH)) << 9
         : D == NorthWest    ? (b & ~fileBB(FileA)) << 7
         : D == SouthEast    ? (b & ~fileBB(FileH)) >> 7
         : D == SouthWest    ? (b & ~fileBB(FileA)) >> 9
                              : 0;
}

constexpr static BB indexToBB(Square sq) noexcept {
    return 1ULL << sq;
}
constexpr BB indexToBB(int idx) noexcept {
    return 1ULL << idx;
}
template <int index>
constexpr static BB toBB() noexcept { return 1ULL << index; }

static inline Square lsb(u64 bb) noexcept {
#ifdef __linux__
    return (Square) __builtin_ctzll(bb);
#else
    unsigned long idx;
    _BitScanForward64(&idx, bb);
    return (Square) idx;
#endif
}
static inline int lsbIndex(u64 bb) noexcept {
#ifdef __linux__
    return __builtin_ctzll(bb);
#else
    unsigned long idx;
    _BitScanForward64(&idx, bb);
    return idx;
#endif
}
inline int popCount(u64 bb) noexcept {
#ifdef __linux__
    return __builtin_popcountll(bb);
#else
    return __popcnt64(bb);
#endif
}
constexpr inline u64 setAllBit(bool comp) noexcept {
    return static_cast<u64>(-comp);
}
constexpr inline u32 setAllBit32(bool comp) noexcept {
    return static_cast<u32>(-comp);
}

// Finds and clears the least significant bit in a non-zero bitboard.
inline Square popLsb(BB& b) noexcept {
    const Square s = lsb(b);
    b &= b - 1;
    return s;
}

constexpr inline bool isMoreThanOne(BB b) noexcept {
    return b & (b - 1);
}

enum CastlingRights : int {
    NoCastling = 0,
    CastlingWK = 1,
    CastlingWQ = 2,
    CastlingBK = 4,
    CastlingBQ = 8,
    CastlingWhite = CastlingWK | CastlingWQ,
    CastlingBlack = CastlingBK | CastlingBQ,
    CastlingRightsAny = CastlingWhite | CastlingBlack,
    CastlingRights_NB = 16
};

template <CastlingRights c>
constexpr static inline Color castlingRightToColor() noexcept {
    if constexpr (c == CastlingWK || c == CastlingWQ){
        return White;
    } else {
        return Black;
    }
}

template <CastlingRights c, PieceType type, bool isSrc>
constexpr static Square getCastlingIndex() noexcept {
    if constexpr (type == King){
        if constexpr (c == CastlingWK){
            return isSrc ? E1 : G1;
        } else if constexpr (c == CastlingWQ){
            return isSrc ? E1 : C1;
        } else if constexpr (c == CastlingBK){
            return isSrc ? E8 : G8;
        } else if constexpr (c == CastlingBQ){
            return isSrc ? E8 : C8;
        }
    } else if constexpr (type == Rook){
        if constexpr (c == CastlingWK){
            return isSrc ? H1 : F1;
        } else if constexpr (c == CastlingWQ){
            return isSrc ? A1 : D1;
        } else if constexpr (c == CastlingBK){
            return isSrc ? H8 : F8;
        } else if constexpr (c == CastlingBQ){
            return isSrc ? A8 : D8;
        }
    } else if constexpr (type == Knight){ // Sử dụng để tính & trả về vị trí mà quân xe đi ngang qua quân mã khi nhập thành
        if constexpr (c == CastlingWQ){
            return B1;
        } else if constexpr (c == CastlingBQ){
            return B8;
        }
    }
}

template<CastlingRights c, PieceType type>
constexpr static BB getCastlingToggle() noexcept {
    return toBB<getCastlingIndex<c, type, true>()>() | toBB<getCastlingIndex<c, type, false>()>();
}

template<CastlingRights c>
constexpr static BB castlingKingPath() noexcept {
    return toBB<getCastlingIndex<c, King, true>()>() 
        | toBB<getCastlingIndex<c, Rook, false>()>()
        | toBB<getCastlingIndex<c, King, false>()>();
}
template<CastlingRights c>
constexpr static BB castlingSpace() noexcept {
    if constexpr (c == CastlingWK || c == CastlingBK){
        return toBB<getCastlingIndex<c, King, false>()>()
            | toBB<getCastlingIndex<c, Rook, false>()>();
    } else {
        return toBB<getCastlingIndex<c, King, false>()>()
            | toBB<getCastlingIndex<c, Rook, false>()>()
            | toBB<getCastlingIndex<c, Knight, false>()>();
    }
}

struct Move {
    enum {
        NullMove = 0,
        FromToBit = 6, SpecialBit = 12,
    };
    enum Flag : u16 {
        Quiet           = (0 << SpecialBit),
        PawnPush2       = (1 << SpecialBit),
        KingCastling    = (2 << SpecialBit),
        QueenCastling   = (3 << SpecialBit),
        Capture         = (4 << SpecialBit),
        Enpassant       = (5 << SpecialBit),
        Promotion       = (8 << SpecialBit),
        CapturePromotion= Capture + Promotion,
        All             = (15 << SpecialBit),
    };
    constexpr inline Move() : move(NullMove), val(0){}

    constexpr inline Move(int src, int dst, Flag flag = Quiet)
        : move(flag + src + (dst << FromToBit)), val(0){}

    constexpr inline Move(int src, int dst, Flag flag, PieceType p)
        : move(flag + src + (dst << FromToBit) + ((p - 2) << 12)), val(0){}

    constexpr inline u16 flag() const noexcept {
        return move & All;
    }

    template <Flag f>
    constexpr inline bool is() const noexcept {
        if constexpr (f == Capture || f == Promotion){
            return f & flag(); // Có nhiều loại capture & promotion
        } else {
            return f == flag();
        }
    }

    constexpr inline int src() const noexcept { return move & 0x3f; }
    constexpr inline int dst() const noexcept { return (move >> 6) & 0x3f; }
    constexpr inline PieceType getPromotionPieceType() const noexcept { return PieceType(((move >> 12) & 0x03) + 2); }

    std::string getDescription() const noexcept ;
private:
    u16 move;
public:
    i16 val;
};

enum Score {
    ScoreMate = 32000,
};

}
















