#include "board.h"
#include <math.h>

void kchess::Board::doMove(Move move){
    const auto &src = move.src();
    const auto &dst = move.dst();
    auto pieceSrc = pieces[src];
    auto srcColor = pieceToColor(pieceSrc);
    auto srcType = pieceToType(pieceSrc);

    auto enemyColor = !srcColor;

    const auto &srcBB = squareToBB(src);
    const auto &dstBB = squareToBB(dst);
    const auto &toggleBB = srcBB | dstBB;

    // Di chuyển bitboard (xóa một ô trong trường hợp ô đến trống)
    auto pieceDst = pieces[dst];
    if (pieceDst != PieceNone){
        auto dstColor = pieceToColor(pieceDst);
        auto dstType = pieceToType(pieceDst);
        colors[dstColor] &= ~dstBB;
        types[dstType] &= ~dstBB;
    }

    colors[srcColor] ^= toggleBB;
    types[srcType] ^= toggleBB;


    // Di chuyển ô
    pieces[dst] = pieceSrc;
    pieces[src] = PieceNone;


    // Thực hiện nhập thành
    if (move.type() == Move::Castling){
        if (move.dst() == CastlingWKOO){
            colors[White] ^= CastlingWROO_Toggle;
            types[Rook] ^= CastlingWROO_Toggle;
            pieces[_H1] = PieceNone;
            pieces[_F1] = WhiteRook;
        } else if (move.dst() == CastlingWKOOO){
            colors[White] ^= CastlingWROOO_Toggle;
            types[Rook] ^= CastlingWROOO_Toggle;
            pieces[_A1] = PieceNone;
            pieces[_D1] = WhiteRook;
        } else if (move.dst() == CastlingBKOO){
            colors[Black] ^= CastlingBROO_Toggle;
            types[Rook] ^= CastlingBROO_Toggle;
            pieces[_H8] = PieceNone;
            pieces[_F8] = BlackRook;
        } else if (move.dst() == CastlingBKOOO){
            colors[Black] ^= CastlingBROOO_Toggle;
            types[Rook] ^= CastlingBROOO_Toggle;
            pieces[_A8] = PieceNone;
            pieces[_D8] = BlackRook;
        }
    }

    // Đánh dấu lại cờ nhập thành
    if (pieceSrc == WhiteKing){
        whiteOO = false;
        whiteOOO = false;
    } else if (pieceSrc == WhiteRook){
        if (src == _H1){
            whiteOO = false;
        } else if (src == _A1){
            whiteOOO = false;
        }
    } else if (pieceDst == WhiteRook){
        if (dst == _H1){
            whiteOO = false;
        } else if (dst == _A1){
            whiteOOO = false;
        }
    } else if (pieceSrc == BlackKing){
        blackOO = false;
        blackOOO = false;
    } else if (pieceSrc == BlackRook){
        if (src == _H8){
            blackOO = false;
        } else if (src == _A8) {
            blackOOO = false;
        }
    } else if (pieceDst == BlackRook){
        if (dst == _H8){
            blackOO = false;
        } else if (dst == _A8){
            blackOOO = false;
        }
    }

    // Cập nhật trạng thái tốt qua đường
    if (pieceToType(pieceSrc) == Pawn && abs(src - dst) == 16){
        enPassant = Square(dst);
    } else {
        enPassant = SquareNone;
    }

    if (move.type() == Move::Enpassant){
        int enemyPawn = srcColor == White ? dst - 8 : dst + 8;
        BB enemyPawnBB = squareToBB(enemyPawn);

        colors[enemyColor] &= ~enemyPawnBB;
        types[Pawn] &= ~enemyPawnBB;
        pieces[enemyPawn] = PieceNone;
    }


    // Chuyển màu
    side = !side;

}

























