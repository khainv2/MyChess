#include "board.h"
#include <math.h>
#include "util.h"
#include "evaluation.h"
#include "attack.h"

using namespace kc;

template <Color color>
BB kc::Board::getCheckMask() const {
    BB ourKing = types[King] & colors[color];

    BB occ = occupancy();
    BB enem = enemies();

    const BB &pawns = types[Pawn];
    const BB &knights = types[Knight];
    const BB &bishops = types[Bishop];
    const BB &rooks = types[Rook];
    const BB &queens = types[Queen];
    const BB &kings = types[King];

    // Kiểm tra xem các tốt có đang chiếu hay không

    BB enemiesPawn = enem & pawns;
//    BB enemiesPawnAttack = (enemiesPawn << 7)


    int countCheck = 0;
    for (int i = 0; i < Square_Count; i++){
        u64 from = squareToBB(i);
        if ((from & enem) == 0)
            continue;
        if (from & pawns){
            if (attack::pawns[!side][i] & ourKing){
                countCheck++;
            }
        } else if (from & knights){
            if (attack::knights[i] & ourKing){
                countCheck++;
            }
        } else if (from & bishops){
            BB eAttack = attack::getBishopAttacks(i, occ);
            if (eAttack & ourKing){
                countCheck++;
            }
        } else if (from & rooks){
            BB eAttack = attack::getRookAttacks(i, occ);
            if (eAttack & ourKing){
                countCheck++;
            }
        } else if (from & queens){
            BB eAttack = attack::getQueenAttacks(i, occ);
            if (eAttack & ourKing){
                countCheck++;
            }
        }
    }
    return countCheck;
}

int kc::Board::doMove(Move move){
    int capture = 0;
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
        capture = 1;
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
        capture = 1;
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
    return capture;
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


























