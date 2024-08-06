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
        return colors[state.getColorActive()];
    }
    constexpr Bitboard enemies() const { 
        return colors[1 - state.getColorActive()];
    }
    void toggleActive(){
        state.setToggleActive();
    }

    // create pieceToChar
    char pieceToChar(Piece p, Color color) const {
        if (color == Color::Black){
            switch (p){
                case Piece::Pawn: return 'P';
                case Piece::Knight: return 'N';
                case Piece::Bishop: return 'B';
                case Piece::Rook: return 'R';
                case Piece::Queen: return 'Q';
                case Piece::King: return 'K';
                default: return '?';
            }
        } else {
            switch (p){
                case Piece::Pawn: return 'p';
                case Piece::Knight: return 'n';
                case Piece::Bishop: return 'b';
                case Piece::Rook: return 'r';
                case Piece::Queen: return 'q';
                case Piece::King: return 'k';
                default: return '?';
            }
        }
    }

    std::string getPrintable() const {
        std::string res = "";
        for (int i = 0; i < 64; i++){
            if (i % 8 == 0) res += "\n";
            bool found = false;
            for (int j = 0; j < Piece_NB; j++){
                if (pieces[j] & squareToBB(i)){
                    res += pieceToChar(Piece(j), Color((colors[Color::White] & squareToBB(i)) != 0));
                    found = true;
                    break;
                }
            }
            if (!found) res += ".";
        }
        return res;
    }

//    void doMove(Move move){
//        int src = getSourceFromMove(move);
//        int dst = getDestinationFromMove(move);
//        u64 srcBB = squareToBB(src);
//        u64 dstBB = squareToBB(dst);
//        u64 toggle = srcBB | dstBB;
//        u64 notDstBB = ~dstBB;
//        pieces[Piece::Pawn] &= notDstBB;
//        pieces[Piece::Knight] &= notDstBB;
//        pieces[Piece::Bishop] &= notDstBB;
//        pieces[Piece::Rook] &= notDstBB;
//        pieces[Piece::Queen] &= notDstBB;
//        pieces[Piece::King] &= notDstBB;
//        colors[Color::Black] &= notDstBB;
//        colors[Color::White] &= notDstBB;

//        pieces[Piece::Pawn] ^= (toggle & (u64((pieces[Piece::Pawn] & srcBB) == 0) - 1));
//        pieces[Piece::Knight] ^= (toggle & (u64((pieces[Piece::Knight] & srcBB) == 0) - 1));
//        pieces[Piece::Bishop] ^= (toggle & (u64((pieces[Piece::Bishop] & srcBB) == 0) - 1));
//        pieces[Piece::Rook] ^= (toggle & (u64((pieces[Piece::Rook] & srcBB) == 0) - 1));
//        pieces[Piece::Queen] ^= (toggle & (u64((pieces[Piece::Queen] & srcBB) == 0) - 1));
//        pieces[Piece::King] ^= (toggle & (u64((pieces[Piece::King] & srcBB) == 0) - 1));
//        colors[Color::Black] ^= (toggle & (u64((colors[Color::Black] & srcBB) == 0) - 1));
//        colors[Color::White] ^= (toggle & (u64((colors[Color::White] & srcBB) == 0) - 1));
//        toggleActive();
//    }
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
