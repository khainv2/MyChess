#include "board.h"
#include <math.h>
#include "util.h"
#include "evaluation.h"
#include "attack.h"

using namespace kc;

Board::Board() noexcept {
    //    state = new BoardState;
}

Board::~Board() noexcept {
    //    delete state;
}

BB Board::getSqAttackTo(int sq, BB occ) const noexcept {
    return (attack::pawns[Black][sq] & getPieceBB<White, Pawn>())
         | (attack::pawns[White][sq] & getPieceBB<Black, Pawn>())
         | (attack::knights[sq] & getPieceBB<Knight>())
         | (attack::getBishopAttacks(sq, occ) & getPieceBB<Bishop, Queen>())
         | (attack::getRookAttacks(sq, occ) & getPieceBB<Rook, Queen>())
//         | (attack::kings[sq] & getPieceBB<King>())
            ;
}

int kc::Board::doMove(Move move, BoardState &newState) noexcept {
    newState.castlingRights = state->castlingRights;
    newState.enPassant = state->enPassant;
    newState.halfMoveClock = state->halfMoveClock;
    newState.fullMoveNumber = state->fullMoveNumber;

    newState.previous = state;
    state = &newState;

    const int src = move.src();
    const int dst = move.dst();

    auto pieceSrc = pieces[src];
    auto srcColor = pieceToColor(pieceSrc);
    auto srcType = pieceToType(pieceSrc);

    auto enemyColor = !srcColor;

    const auto &srcBB = indexToBB(src);
    const auto &dstBB = indexToBB(dst);
    const auto &toggleBB = srcBB | dstBB;

    // Di chuyển bitboard (xóa một ô trong trường hợp ô đến trống)
    auto pieceDst = pieces[dst];
    if (pieceDst != PieceNone){
        state->capturedPiece = pieceDst;
        auto dstColor = pieceToColor(pieceDst);
        auto dstType = pieceToType(pieceDst);
        colors[dstColor] &= ~dstBB;
        types[dstType] &= ~dstBB;
    } else {
        state->capturedPiece = PieceNone;
    }

    colors[srcColor] ^= toggleBB;
    types[srcType] ^= toggleBB;

    // Di chuyển ô
    pieces[dst] = pieceSrc;
    pieces[src] = PieceNone;

    // Thực hiện nhập thành
    if (move.type() == Move::Castling){
        constexpr auto static testCastlingWK = getCastlingIndex<CastlingWK, King, false>();
        constexpr auto static testCastlingWQ = getCastlingIndex<CastlingWQ, King, false>();
        constexpr auto static testCastlingBK = getCastlingIndex<CastlingBK, King, false>();
        constexpr auto static testCastlingBQ = getCastlingIndex<CastlingBQ, King, false>();

        if (move.dst() == testCastlingWK){
            constexpr auto static toggle = getCastlingToggle<CastlingWK, Rook>();
            constexpr auto static rookSrc = getCastlingIndex<CastlingWK, Rook, true>();
            constexpr auto static rookDst = getCastlingIndex<CastlingWK, Rook, false>();
            colors[White] ^= toggle;
            types[Rook] ^= toggle;
            pieces[rookSrc] = PieceNone;
            pieces[rookDst] = WhiteRook;
        } else if (move.dst() == testCastlingWQ){
            constexpr auto static toggle = getCastlingToggle<CastlingWQ, Rook>();
            constexpr auto static rookSrc = getCastlingIndex<CastlingWQ, Rook, true>();
            constexpr auto static rookDst = getCastlingIndex<CastlingWQ, Rook, false>();
            colors[White] ^= toggle;
            types[Rook] ^= toggle;
            pieces[rookSrc] = PieceNone;
            pieces[rookDst] = WhiteRook;
        } else if (move.dst() == testCastlingBK){
            constexpr auto static toggle = getCastlingToggle<CastlingBK, Rook>();
            constexpr auto static rookSrc = getCastlingIndex<CastlingBK, Rook, true>();
            constexpr auto static rookDst = getCastlingIndex<CastlingBK, Rook, false>();
            colors[Black] ^= toggle;
            types[Rook] ^= toggle;
            pieces[rookSrc] = PieceNone;
            pieces[rookDst] = BlackRook;
        } else if (move.dst() == testCastlingBQ){
            constexpr auto static toggle = getCastlingToggle<CastlingBQ, Rook>();
            constexpr auto static rookSrc = getCastlingIndex<CastlingBQ, Rook, true>();
            constexpr auto static rookDst = getCastlingIndex<CastlingBQ, Rook, false>();
            colors[Black] ^= toggle;
            types[Rook] ^= toggle;
            pieces[rookSrc] = PieceNone;
            pieces[rookDst] = BlackRook;
        }
    }

    // Đánh dấu lại cờ nhập thành
    if (pieceSrc == WhiteKing){
        state->castlingRights &= ~CastlingWhite;
    } else if (pieceSrc == BlackKing){
        state->castlingRights &= ~CastlingBlack;
    }

    if (pieceSrc == WhiteRook || pieceDst == WhiteRook){
        if (src == H1 || dst == H1){
            state->castlingRights &= ~CastlingWK;
        } else if (src == A1 || dst == A1){
            state->castlingRights &= ~CastlingWQ;
        }
    }

    if (pieceSrc == BlackRook || pieceDst == BlackRook){
        if (src == H8 || dst == H8){
            state->castlingRights &= ~CastlingBK;
        } else if (src == A8 || dst == A8){
            state->castlingRights &= ~CastlingBQ;
        }
    }

    // Cập nhật trạng thái tốt qua đường
    int enPassantCondition = srcType == Pawn && abs(src - dst) == 16;
    state->enPassant = Square(enPassantCondition * dst);

    if (move.type() == Move::Enpassant){
        int enemyPawn = srcColor == White ? dst - 8 : dst + 8;
        BB enemyPawnBB = indexToBB(enemyPawn);
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

    state->halfMoveClock = (srcType == Pawn || pieceDst != PieceNone) ? 0 : state->halfMoveClock + 1;
    state->fullMoveNumber += srcColor;

    // Chuyển màu
    side = !side;
    return 0;
}

int Board::undoMove(Move move) noexcept
{
    const int src = move.src();
    const int dst = move.dst();

    auto pieceDst = pieces[dst];
    auto dstColor = pieceToColor(pieceDst);
    auto dstType = pieceToType(pieceDst);

    const auto &srcBB = indexToBB(src);
    const auto &dstBB = indexToBB(dst);
    const auto &toggleBB = srcBB | dstBB;

    colors[dstColor] ^= toggleBB;
    types[dstType] ^= toggleBB;

    pieces[src] = pieceDst;
    
    if (state->capturedPiece != PieceNone){
        auto dstColor = pieceToColor(state->capturedPiece);
        auto dstType = pieceToType(state->capturedPiece);
        colors[dstColor] |= indexToBB(dst);
        types[dstType] |= indexToBB(dst);
        pieces[dst] = state->capturedPiece;
    } else {
        pieces[dst] = PieceNone;
    }

    if (move.type() == Move::Castling){
        constexpr static auto testCastlingWK = getCastlingIndex<CastlingWK, King, false>();
        constexpr static auto testCastlingWQ = getCastlingIndex<CastlingWQ, King, false>();
        constexpr static auto testCastlingBK = getCastlingIndex<CastlingBK, King, false>();
        constexpr static auto testCastlingBQ = getCastlingIndex<CastlingBQ, King, false>();
        if (move.dst() == testCastlingWK){
            constexpr static auto toggle = getCastlingToggle<CastlingWK, Rook>();
            constexpr static auto rookSrc = getCastlingIndex<CastlingWK, Rook, true>();
            constexpr static auto rookDst = getCastlingIndex<CastlingWK, Rook, false>();
            colors[White] ^= toggle;
            types[Rook] ^= toggle;
            pieces[rookSrc] = WhiteRook;
            pieces[rookDst] = PieceNone;
        } else if (move.dst() == testCastlingWQ){
            constexpr static auto toggle = getCastlingToggle<CastlingWQ, Rook>();
            constexpr static auto rookSrc = getCastlingIndex<CastlingWQ, Rook, true>();
            constexpr static auto rookDst = getCastlingIndex<CastlingWQ, Rook, false>();
            colors[White] ^= toggle;
            types[Rook] ^= toggle;
            pieces[rookSrc] = WhiteRook;
            pieces[rookDst] = PieceNone;
        } else if (move.dst() == testCastlingBK){
            constexpr static auto toggle = getCastlingToggle<CastlingBK, Rook>();
            constexpr static auto rookSrc = getCastlingIndex<CastlingBK, Rook, true>();
            constexpr static auto rookDst = getCastlingIndex<CastlingBK, Rook, false>();
            colors[Black] ^= toggle;
            types[Rook] ^= toggle;
            pieces[rookSrc] = BlackRook;
            pieces[rookDst] = PieceNone;
        } else if (move.dst() == testCastlingBQ){
            constexpr static auto toggle = getCastlingToggle<CastlingBQ, Rook>();
            constexpr static auto rookSrc = getCastlingIndex<CastlingBQ, Rook, true>();
            constexpr static auto rookDst = getCastlingIndex<CastlingBQ, Rook, false>();
            colors[Black] ^= toggle;
            types[Rook] ^= toggle;
            pieces[rookSrc] = BlackRook;
            pieces[rookDst] = PieceNone;
        }
    }

    if (move.type() == Move::Enpassant){
        int enemyPawn = dstColor == White ? dst - 8 : dst + 8;
        BB enemyPawnBB = indexToBB(enemyPawn);
        colors[!dstColor] |= enemyPawnBB;
        types[Pawn] |= enemyPawnBB;
        pieces[enemyPawn] = makePiece(!dstColor, Pawn);
    }

    if (move.type() == Move::Promotion){
        pieces[src] = makePiece(dstColor, Pawn);
        types[dstType] ^= srcBB;
        types[Pawn] |= srcBB;
    }

    state = state->previous;

    side = !side;

    return 0;
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
                if (state->castlingRights & CastlingWK){
                    str += "K";
                }
                if (state->castlingRights & CastlingWQ){
                    str += "Q";
                }
                if (state->castlingRights & CastlingBK){
                    str += "k";
                }
                if (state->castlingRights & CastlingBQ){
                    str += "q";
                }
                if (str.at(str.size() - 1) == ' '){
                    str += "-";
                }
                str += " ";

                if (state->enPassant == SquareNone){
                    str += "-";
                } else {
                    auto r = getRank(state->enPassant);
                    auto f = getFile(state->enPassant);

                    str += fileToChar(f);
                    str += rankToChar(r);
                }
                str += " ";
                str += std::to_string(state->halfMoveClock);
                str += " ";
                str += std::to_string(state->fullMoveNumber);

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


























