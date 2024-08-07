#pragma once
#include "define.h"
#include <QDebug>
namespace kchess {

struct Board {
    Piece pieces[Square_Count] = { PieceNone };
    BB colors[Color_NB] = {0};
    BB types[PieceType_NB] = {0};

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

    Board(){}

    constexpr inline BB occupancy() const {
        return colors[White] | colors[Black]; 
    }
    constexpr inline BB occupancy(Color color) const {
        return colors[color];
    }
    constexpr inline BB mines() const {
        return colors[side];
    }
    constexpr inline BB notMines() const {
        return ~colors[side];
    }
    constexpr inline BB enemies() const {
        return colors[1 - side];
    }
    constexpr inline BB notEnemies() const {
        return ~colors[1 - side];
    }

    void doMove(Move move);

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
