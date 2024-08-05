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
        int src = getSourceFromMove(move);
        int dst = getDestinationFromMove(move);
        u64 srcBB = squareToBB(src);
        u64 dstBB = squareToBB(dst);

        for (int i = 0; i < Color_NB; i++){
            colors[i] |= dstBB;
            colors[i] &= ~srcBB;
        }
        auto color = getColorActive(state);

        colors[color] |= dstBB;
        colors[color] &= ~srcBB;
        pieces[Piece::Pawn] |= dstBB;
        pieces[Piece::Pawn] &= ~srcBB;
        pieces[Piece::Knight] |= dstBB;
        pieces[Piece::Knight] &= ~srcBB;
        pieces[Piece::Bishop] |= dstBB;
        pieces[Piece::Bishop] &= ~srcBB;
        pieces[Piece::Rook] |= dstBB;
        pieces[Piece::Rook] &= ~srcBB;
        pieces[Piece::Queen] |= dstBB;
        pieces[Piece::Queen] &= ~srcBB;
        pieces[Piece::King] |= dstBB;
        pieces[Piece::King] &= ~srcBB;
        toggleActive();
    }
    // void doMove(Move move){
    //     auto srcBB = squareToBB(getSourceFromMove(move));
    //     auto dstBB = squareToBB(getDestinationFromMove(move));
    //     auto bblist = reinterpret_cast<u64 *>(this);
    //     for (int i = 0; i < 8; i++){
    //         auto v = bblist[i] & srcBB;
    //         if (v) bblist[i] |= dstBB;
    //         else bblist[i] &= (~dstBB);
    //         bblist[i] &= (~srcBB);
    //     }
    //     toggleActive();
    // }
};
}
