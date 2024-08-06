#pragma once
#include "define.h"
namespace kchess {

struct ChessBoard {
    Piece pieces[Square_Count] = { PieceNone };
    Bitboard colors[Color_NB] = {0};
    Bitboard types[PieceType_NB] = {0};

    Color side = White;

    bool whiteOOO = true;
    bool whiteOO = true;
    bool blackOOO = true;
    bool blackOO = true;

    // En passant target square
    Square enPassant = _A1;
    // Half move clock
    int halfMoveClock = 0;
    // Full move number
    int fullMoveNumber = 0;

    ChessBoard(){}

    constexpr inline Bitboard occupancy() const { 
        return colors[White] | colors[Black]; 
    }
    constexpr inline Bitboard occupancy(Color color) const { 
        return colors[color];
    }
    constexpr inline Bitboard mines() const { 
        return colors[side];
    }
    constexpr inline Bitboard notMines() const { 
        return ~colors[side];
    }
    constexpr inline Bitboard enemies() const { 
        return colors[1 - side];
    }
    constexpr inline Bitboard notEnemies() const { 
        return ~colors[1 - side];
    }


     void doMove(Move move){
         auto src = move.src();
         auto dst = move.dst();
         auto piece = pieces[src];
         auto color = pieceToColor(piece);
         auto type = pieceToType(piece);
         auto toggle = squareToBB(src) | squareToBB(dst);
         pieces[dst] = piece;
         pieces[src] = PieceNone;
         colors[color] ^= toggle;
         types[type] ^= toggle;
         side = !side;
     }

     std::string getPrintable() const {
         std::string res = "";
         for (int i = 0; i < 64; i++){
             if (i % 8 == 0) res += "\n";
             for (int j = 0; j < Piece_NB; j++){
                 if (types[j] & squareToBB(i)){
                     res += pieceToChar(Piece(j));
                     break;
                 }
             }
         }
         return res;
     }
};
}
