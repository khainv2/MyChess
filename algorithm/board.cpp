#include "board.h"
#include <math.h>
#include "util.h"
#include "evaluation.h"

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

std::string kc::Board::getPrintable(int tab) const {
    std::string newLine = "\n";
    while (tab > 0){
        newLine += "\t";
        tab--;
    }
    std::string str = newLine;

    for (int index = 0; index < 64; index++){
        int r = index / 8;
        int c = index % 8;
        int i = (7 - r) * 8 + c;
        str += (pieceToChar(pieces[i]));
        str += " ";
        if (index % 8 == 7){
            if (index == 15){
                str += " | "; str += (side == White ? "w" : "b");
                str += " ";
                if (whiteOO){
                    str += "K";
                }
                if (whiteOOO){
                    str += "Q";
                }
                if (blackOO){
                    str += "k";
                }
                if (blackOOO){
                    str += "q";
                }
                if (str.at(str.size() - 1) == ' '){
                    str += "-";
                }
                str += " ";

                if (enPassant == SquareNone){
                    str += "-";
                } else {
                    auto r = getRank(enPassant);
                    auto f = getFile(enPassant);

                    str += fileToChar(f);
                    str += rankToChar(r);
                }
                str += " ";
                str += std::to_string(halfMoveClock);
                str += " ";
                str += std::to_string(fullMoveNumber);

            }

            if (index == 31){
                str += " | eval: ";
                str += std::to_string(eval::estimate(*this));
            }

            if (index != 63)
                str += newLine;
        }
    }
    return str;
}

























