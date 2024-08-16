#include "board.h"
#include <math.h>
#include "util.h"
#include "evaluation.h"
#include "attack.h"

using namespace kc;

Board::Board(){
//    state = new BoardState;
}

Board::~Board()
{
//    delete state;
}

int kc::Board::doMove(Move move, BoardState &newState){
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
    if ((pieces[E1] == WhiteKing) && (pieces[H1] == WhiteRook)){
        state->castlingRights |= CastlingRights::CastlingWK;
    }
    if ((pieces[E1] == WhiteKing) && (pieces[A1] == WhiteRook)){
        state->castlingRights |= CastlingRights::CastlingWQ;
    }
    if ((pieces[E8] == BlackKing) && (pieces[H8] == BlackRook)){
        state->castlingRights |= CastlingRights::CastlingBK;
    }
    if ((pieces[E1] == WhiteKing) && (pieces[H1] == WhiteRook)){
        state->castlingRights |= CastlingRights::CastlingBQ;
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
//        capture = 1;
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

int Board::undoMove(Move move)
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
        if (move.dst() == CastlingWKOO){
            colors[White] ^= CastlingWROO_Toggle;
            types[Rook] ^= CastlingWROO_Toggle;
            pieces[H1] = WhiteRook;
            pieces[F1] = PieceNone;
        } else if (move.dst() == CastlingWKOOO){
            colors[White] ^= CastlingWROOO_Toggle;
            types[Rook] ^= CastlingWROOO_Toggle;
            pieces[A1] = WhiteRook;
            pieces[D1] = PieceNone;
        } else if (move.dst() == CastlingBKOO){
            colors[Black] ^= CastlingBROO_Toggle;
            types[Rook] ^= CastlingBROO_Toggle;
            pieces[H8] = BlackRook;
            pieces[F8] = PieceNone;
        } else if (move.dst() == CastlingBKOOO){
            colors[Black] ^= CastlingBROOO_Toggle;
            types[Rook] ^= CastlingBROOO_Toggle;
            pieces[A8] = BlackRook;
            pieces[D8] = PieceNone;
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
        types[dstColor] ^= srcBB;
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


























