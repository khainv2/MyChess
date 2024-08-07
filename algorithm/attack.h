#pragma once
#include "define.h"
namespace kc {
namespace attack {

extern u64 kings[Square_Count];
extern u64 knights[Square_Count];
extern u64 pawns[Color_NB][Square_Count];
extern u64 pawnPushes[Color_NB][Square_Count];
extern u64 pawnPushes2[Color_NB][Square_Count];
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
constexpr u64 oneSquareAttack(u64 square, u64 border){
    if (square & border){
        return 0;
    } else {
        if constexpr (shift > 0)
            return square << shift;
        else
            return square >> -shift;
    }
}
constexpr u64 oneSquareAttack(u64 square, int shift, u64 border){
    if ((square & border) == 0){
        if (shift > 0) square <<= shift;
        else square >>= -shift;
        return square;
    }
    return 0;
}
constexpr u64 getSlideAttack(u64 square, int shift, u64 border){
    u64 t = 0;
    while ((square & border) == 0) {
        if (shift > 0) square <<= shift;
        else square >>= -shift;
        t |= square;
    }
    return t;
}

u64 getRookAttacks(int index, u64 occ);
u64 getBishopAttacks(int index, u64 all);
u64 getPawnAttacks(int index, u64 occ);
u64 getQueenAttacks(int index, u64 occ);
}
}
