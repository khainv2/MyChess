#pragma once
#include "define.h"
#include <QDebug>
namespace kc {

struct Board {
    Piece pieces[Square_Count] = { PieceNone };
    BB colors[Color_NB] = {0};
    BB types[PieceType_NB] = {0};

    bool isPieceAndColorTypeMatch() const {
        for (int i = 0; i < Square_Count; i++){
            auto piece = pieces[i];
            if (piece == PieceNone)
                continue;
            auto color = pieceToColor(piece);
            auto type = pieceToType(piece);
            if (!(colors[color] & squareToBB(i)) || !(types[type] & squareToBB(i))){
                qDebug() << "Piece and color/type mismatch at square " << i;
                return false;
            }
        }
        return true;
    }

    Color side = White;

    bool whiteOOO = true;
    bool whiteOO = true;
    bool blackOOO = true;
    bool blackOO = true;

    // En passant target square
    Square enPassant = SquareNone;
    // Half move clock
    int halfMoveClock = 0;
    // Full move number
    int fullMoveNumber = 0;

    Board(){}

    constexpr inline int enPassantTarget(Color color) const {
        return color == White ? enPassant + 8 : enPassant - 8;
    }
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

    template <Color color>
    BB getCheckMask() const;

    int doMove(Move move);

    std::string getPrintable(int tab = 0) const;
};
}
