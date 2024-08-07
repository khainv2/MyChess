#include "board.h"
#include <math.h>

void kc::Board::doMove(Move move){
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
            pieces[H1] = PieceNone;
            pieces[F1] = WhiteRook;
        } else if (move.dst() == CastlingWKOOO){
            colors[White] ^= CastlingWROOO_Toggle;
            types[Rook] ^= CastlingWROOO_Toggle;
            pieces[A1] = PieceNone;
            pieces[D1] = WhiteRook;
        } else if (move.dst() == CastlingBKOO){
            colors[Black] ^= CastlingBROO_Toggle;
            types[Rook] ^= CastlingBROO_Toggle;
            pieces[H8] = PieceNone;
            pieces[F8] = BlackRook;
        } else if (move.dst() == CastlingBKOOO){
            colors[Black] ^= CastlingBROOO_Toggle;
            types[Rook] ^= CastlingBROOO_Toggle;
            pieces[A8] = PieceNone;
            pieces[D8] = BlackRook;
        }
    }

    // Đánh dấu lại cờ nhập thành
    whiteOO &= (pieces[E1] == WhiteKing) && (pieces[H1] == WhiteRook);
    whiteOOO &= (pieces[E1] == WhiteKing) && (pieces[A1] == WhiteRook);
    blackOO &= (pieces[E8] == BlackKing) && (pieces[H8] == BlackRook);
    blackOOO &= (pieces[E8] == BlackKing) && (pieces[A8] == BlackRook);

    // Cập nhật trạng thái tốt qua đường
    int enPassantCondition = srcType == Pawn && abs(src - dst) == 16;
    enPassant = Square(enPassantCondition * dst);

    if (move.type() == Move::Enpassant){
        int enemyPawn = srcColor == White ? dst - 8 : dst + 8;
        BB enemyPawnBB = squareToBB(enemyPawn);
        colors[enemyColor] &= ~enemyPawnBB;
        types[Pawn] &= ~enemyPawnBB;
        pieces[enemyPawn] = PieceNone;
    }

    // Cập nhật thăng cấp
    if (move.type() == Move::Promotion){
        auto promotionPiece = move.getPromotionPieceType();
        pieces[dst] = makePiece(srcColor, PieceType(promotionPiece));
        types[srcType] &= ~dstBB;
        types[promotionPiece] ^= dstBB;
    }

    halfMoveClock = (srcType == Pawn || pieceDst != PieceNone) ? 0 : halfMoveClock + 1;

    fullMoveNumber += srcColor;

    // Chuyển màu
    side = !side;
}

























