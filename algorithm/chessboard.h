#pragma once
#include "define.h"
namespace kchess {

struct ChessBoard {
    Bitboard colors[Color_NB] = {0};
    Bitboard pieces[Piece_NB] = {0};

    BoardState state = 0;

    ChessBoard(){
    }

    Bitboard getBitBoard(Piece p, Color c) const {
        auto bb = reinterpret_cast<const Bitboard *>(this);
        return bb[c] & bb[p + 2];
    }

    constexpr Bitboard occupancy() const { return colors[White] | colors[Black]; }
    constexpr Bitboard mines() const {
        return colors[getColorActive(state)];
    }
    constexpr Bitboard enemies() const { 
        return colors[1 - getColorActive(state)];
    }
    void toggleActive(){
        state ^= 1;
    }
    void doMove(Move move){
        auto srcBB = squareToBB(getSourceFromMove(move));
        auto dstBB = squareToBB(getDestinationFromMove(move));
        auto bblist = reinterpret_cast<u64 *>(this);
        for (int i = 0; i < 8; i++){
            auto v = bblist[i] & srcBB;
            if (v) bblist[i] |= dstBB;
            else bblist[i] &= (~dstBB);
            bblist[i] &= (~srcBB);
        }
        toggleActive();
    }
};
}
