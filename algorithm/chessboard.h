#pragma once
#include "define.h"
namespace kchess {

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

    constexpr Bitboard occupancy() const { return blacks | whites; }
    constexpr Bitboard mines() const { return getColorActive(state) == White ? whites : blacks; }
    constexpr Bitboard enemies() const { return getColorActive(state) == White ? blacks : whites; }
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
