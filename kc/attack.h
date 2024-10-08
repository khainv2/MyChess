#pragma once
#include "define.h"
namespace kc {
namespace attack {

extern u64 kings[Square_NB];
extern u64 knights[Square_NB];
extern u64 pawns[Color_NB][Square_NB];
extern u64 pawnPushes[Color_NB][Square_NB];
extern u64 pawnPushes2[Color_NB][Square_NB];

extern BB between[Square_NB][Square_NB];

struct MagicBitboard {
    u64 mask = 0;
    u64 magic = 0;
};

constexpr int RookMagicShiftLength = 46;
extern MagicBitboard rookMagicBitboards[64];
extern u64 rooks[64][262144];

constexpr int BishopMagicShiftLength = 46;
extern MagicBitboard bishopMagicBitboards[64];
extern u64 bishops[64][262144];

void init();
void initMagicTable();

template <int shift>
constexpr u64 oneSquareAttack(u64 square, u64 border) noexcept {
    if (square & border){
        return 0;
    } else {
        if constexpr (shift > 0)
            return square << shift;
        else
            return square >> -shift;
    }
}

constexpr u64 oneSquareAttack(u64 square, int shift, u64 border) noexcept {
    if ((square & border) == 0){
        if (shift > 0) square <<= shift;
        else square >>= -shift;
        return square;
    }
    return 0;
}

template <int shift, BB border>
static constexpr inline BB oneAttack(BB square) noexcept {
    if constexpr (shift > 0){
        return (square & ~border) << shift;
    } else {
        return (square & ~border) >> (-shift);
    }
}

constexpr u64 getSlideAttack(u64 square, int shift, u64 border) noexcept {
    u64 t = 0;
    while ((square & border) == 0) {
        if (shift > 0) square <<= shift;
        else square >>= -shift;
        t |= square;
    }
    return t;
}

template <Color color>
static constexpr inline BB getPawnAttacks(BB pawn) noexcept {
    if constexpr (color == White){
        return oneAttack<+7, B_U | B_L>(pawn) | oneAttack<+9, B_U | B_R>(pawn);
    } else {
        return oneAttack<-7, B_D | B_R>(pawn) | oneAttack<-9, B_D | B_L>(pawn);
    }
}

template <Color color>
static constexpr inline BB getPawnPush(BB pawn) noexcept {
    if constexpr (color == White){
        return oneAttack<+8, B_U>(pawn);
    } else {
        return oneAttack<-8, B_D>(pawn);
    }
}

template <Color color>
static constexpr inline BB getPawnPush2(BB pawn) noexcept {
    if constexpr (color == White){
        return oneAttack<+16, ~B_D2>(pawn);
    } else {
        return oneAttack<-16, ~B_U2>(pawn);
    }
}

static constexpr inline BB getKnightAttacks(BB square) noexcept {
    return oneAttack<+17, B_U2 | B_R >(square)
        | oneAttack<+15, B_U2 | B_L >(square)
        | oneAttack<-15, B_D2 | B_R >(square)
        | oneAttack<-17, B_D2 | B_L >(square)
        | oneAttack<+10, B_U  | B_R2>(square)
        | oneAttack< +6, B_U  | B_L2>(square)
        | oneAttack< -6, B_D  | B_R2>(square)
        | oneAttack<-10, B_D  | B_L2>(square);
}

static constexpr inline BB getKingAttacks(BB square) noexcept {
    return oneAttack<+8, B_U>(square)
        | oneAttack<-8, B_D>(square)
        | oneAttack<-1, B_L>(square)
        | oneAttack<+1, B_R>(square)
        | oneAttack<+9, B_U | B_R>(square)
        | oneAttack<+7, B_U | B_L>(square)
        | oneAttack<-7, B_D | B_R>(square)
        | oneAttack<-9, B_D | B_L>(square);
}

inline static BB getRookAttacks(int index, u64 occ) noexcept {
    occ &= rookMagicBitboards[index].mask;
    occ *= rookMagicBitboards[index].magic;
    occ >>= RookMagicShiftLength;
    return rooks[index][occ];
}
inline static BB getBishopAttacks(int index, u64 occ) noexcept {
    occ &= bishopMagicBitboards[index].mask;
    occ *= bishopMagicBitboards[index].magic;
    occ >>= BishopMagicShiftLength;
    return bishops[index][occ];
}
inline static BB getQueenAttacks(int index, u64 occ) noexcept {
    return getBishopAttacks(index, occ) | getRookAttacks(index, occ);
}

inline static BB getRookXRay(int index, BB occ) noexcept {
    BB attack = getRookAttacks(index, occ);
    return getRookAttacks(index, (occ & ~(attack & occ)));
}
inline static BB getBishopXRay(int index, BB occ) noexcept {
    BB attack = getBishopAttacks(index, occ);
    return getBishopAttacks(index, (occ & ~(attack & occ)));
}

}
}
