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

    void doMove(Move move){
        const auto &src = move.src();
        const auto &dst = move.dst();
        auto piece = pieces[src];
        const auto &color = pieceToColor(piece);
        const auto &type = pieceToType(piece);
        const auto &toggle = squareToBB(src) | squareToBB(dst);

        pieces[dst] = piece;
        pieces[src] = PieceNone;
        colors[color] ^= toggle;
        types[type] ^= toggle;
        side = !side;

        // Thực hiện nhập thành
        if (move.type() == Move::Castling){
            if (move.dst() == CastlingWKOO){
                colors[color] ^= CastlingWROO_Toggle;
                types[color] ^= CastlingWROO_Toggle;
                pieces[_H1] = PieceNone;
                pieces[_F1] = WhiteRook;
            } else if (move.dst() == CastlingWKOOO){
                colors[color] ^= CastlingWROOO_Toggle;
                types[color] ^= CastlingWROOO_Toggle;
                pieces[_A1] = PieceNone;
                pieces[_D1] = WhiteRook;
            } else if (move.dst() == CastlingBKOO){
                colors[color] ^= CastlingBROO_Toggle;
                types[color] ^= CastlingBROO_Toggle;
                pieces[_H8] = PieceNone;
                pieces[_F8] = BlackRook;
            } else if (move.dst() == CastlingBKOOO){
                colors[color] ^= CastlingBROOO_Toggle;
                types[color] ^= CastlingBROOO_Toggle;
                pieces[_A8] = PieceNone;
                pieces[_D8] = BlackRook;
                blackOO = false;
                blackOO = false;
            }
        }

        // Đánh dấu lại cờ nhập thành
        if (piece == WhiteKing){
            whiteOO = false;
            whiteOOO = false;
        } else if (piece == WhiteRook){
            if (src == _H1){
                whiteOO = false;
            } else if (src == _A1){
                whiteOOO = false;
            }
        } else if (piece == BlackKing){
            blackOO = false;
            blackOOO = false;
        } else if (piece == BlackRook){
            if (src == _H8){
                blackOO = false;
            } else if (src == _A8) {
                blackOOO = false;
            }
        }
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
