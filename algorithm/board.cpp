#include "board.h"
#include "tt.h"
#include <math.h>
#include "util.h"
#include "evaluation.h"
#include "attack.h"
#include "movegen.h"

using namespace kc;

Board::Board() noexcept {
}

void Board::initHash() noexcept {
    hash = 0;
    for (int sq = 0; sq < 64; sq++) {
        if (pieces[sq] != PieceNone)
            hash ^= zobrist::pieces[pieces[sq]][sq];
    }
    if (side == Black)
        hash ^= zobrist::side;
    if (state) {
        hash ^= zobrist::castling[state->castlingRights & 0xF];
        if (state->enPassant != SquareNone)
            hash ^= zobrist::enPassant[state->enPassant];
    }
}

Board::~Board() noexcept {
}

bool Board::anyCheck() const noexcept
{
    return state->checkers;
}

bool Board::isMate() const noexcept {
//    int countMove = MoveGen::instance->countMoveList(*this);
    return state->checkers
            && MoveGen::instance->countMoveList(*this) == 0;
}

bool Board::isDraw() const noexcept
{
    return state->halfMoveClock >= 100
            || (state->checkers == 0 && MoveGen::instance->countMoveList(*this) == 0);
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

void kc::Board::doNullMove(BoardState &newState) noexcept {
    // Xóa en passant cũ khỏi hash
    if (state->enPassant != SquareNone)
        hash ^= zobrist::enPassant[state->enPassant];

    newState.castlingRights = state->castlingRights;
    newState.enPassant = SquareNone;
    newState.halfMoveClock = state->halfMoveClock;
    newState.capturedPiece = PieceNone;
    newState.previous = state;
    state = &newState;

    hash ^= zobrist::side;
    side = !side;
    refresh();
}

void kc::Board::undoNullMove() noexcept {
    hash ^= zobrist::side;
    // Khôi phục en passant cũ
    if (state->previous->enPassant != SquareNone)
        hash ^= zobrist::enPassant[state->previous->enPassant];

    state = state->previous;
    side = !side;
}

int kc::Board::doMove(Move move, BoardState &newState) noexcept {
    newState.castlingRights = state->castlingRights;
    newState.enPassant = state->enPassant;
    newState.halfMoveClock = state->halfMoveClock;

    // Xóa castling & en passant cũ khỏi hash
    hash ^= zobrist::castling[state->castlingRights & 0xF];
    if (state->enPassant != SquareNone)
        hash ^= zobrist::enPassant[state->enPassant];

    newState.previous = state;
    state = &newState;

    const int src = move.src();
    const int dst = move.dst();

    const auto pieceSrc = pieces[src];
    const auto srcColor = pieceToColor(pieceSrc);
    const auto srcType = pieceToType(pieceSrc);

    const auto enemyColor = !srcColor;

    const auto srcBB = indexToBB(src);
    const auto dstBB = indexToBB(dst);
    const auto toggleBB = srcBB | dstBB;

    // Di chuyển bitboard (xóa một ô trong trường hợp ô đến trống)
    const auto pieceDst = pieces[dst];
    state->capturedPiece = pieceDst;
    if (pieceDst != PieceNone){
        auto dstColor = pieceToColor(pieceDst);
        auto dstType = pieceToType(pieceDst);
        colors[dstColor] &= ~dstBB;
        types[dstType] &= ~dstBB;
        // Xóa quân bị bắt khỏi hash
        hash ^= zobrist::pieces[pieceDst][dst];
        if (dstColor == White){
        } else {
        }
    }

    colors[srcColor] ^= toggleBB;
    types[srcType] ^= toggleBB;

    // Cập nhật hash: xóa quân ở src, thêm quân ở dst
    hash ^= zobrist::pieces[pieceSrc][src];
    hash ^= zobrist::pieces[pieceSrc][dst];

    // Di chuyển ô
    pieces[dst] = pieceSrc;
    pieces[src] = PieceNone;

    // Thực hiện nhập thành
    if (move.is<Move::KingCastling>()){
        checkAndDoMoveCastling<CastlingBK>(dst);
        checkAndDoMoveCastling<CastlingWK>(dst);
    } else if (move.is<Move::QueenCastling>()){
        checkAndDoMoveCastling<CastlingWQ>(dst);
        checkAndDoMoveCastling<CastlingBQ>(dst);
    } else if (move.is<Move::Enpassant>()){
        int enemyPawn = srcColor == White ? dst - 8 : dst + 8;
        BB enemyPawnBB = indexToBB(enemyPawn);
        colors[enemyColor] &= ~enemyPawnBB;
        types[Pawn] &= ~enemyPawnBB;
        // Xóa tốt bị bắt en passant khỏi hash
        Piece epPiece = makePiece(enemyColor, Pawn);
        hash ^= zobrist::pieces[epPiece][enemyPawn];
        pieces[enemyPawn] = PieceNone;
    } else if (move.is<Move::Promotion>()){
        auto promotionPiece = move.getPromotionPieceType();
        Piece newPiece = makePiece(srcColor, PieceType(promotionPiece));
        // Hash: xóa pawn ở dst (đã thêm ở trên), thêm quân mới
        hash ^= zobrist::pieces[pieceSrc][dst];
        hash ^= zobrist::pieces[newPiece][dst];
        pieces[dst] = newPiece;
        types[srcType] &= ~dstBB;
        types[promotionPiece] ^= dstBB;
    }

    // Đánh dấu lại cờ nhập thành
    state->castlingRights &= ~(CastlingWhite & setAllBit32(pieceSrc == WhiteKing));
    state->castlingRights &= ~(CastlingBlack & setAllBit32(pieceSrc == BlackKing));
    state->castlingRights &= ~(CastlingWK & setAllBit32(src == H1 || dst == H1));
    state->castlingRights &= ~(CastlingWQ & setAllBit32(src == A1 || dst == A1));
    state->castlingRights &= ~(CastlingBK & setAllBit32(src == H8 || dst == H8));
    state->castlingRights &= ~(CastlingBQ & setAllBit32(src == A8 || dst == A8));

    // Cập nhật trạng thái tốt qua đường
    int enPassantCondition = srcType == Pawn && abs(src - dst) == 16;
    state->enPassant = Square(setAllBit32(enPassantCondition) & dst);

    state->halfMoveClock = (srcType == Pawn || pieceDst != PieceNone) ? 0 : state->halfMoveClock + 1;

    // Thêm castling & en passant mới vào hash
    hash ^= zobrist::castling[state->castlingRights & 0xF];
    if (state->enPassant != SquareNone)
        hash ^= zobrist::enPassant[state->enPassant];

    // Đổi bên
    hash ^= zobrist::side;
    side = !side;

    refresh();

    return 0;
}

int Board::undoMove(Move move) noexcept
{
    const int src = move.src();
    const int dst = move.dst();

    const auto pieceDst = pieces[dst];
    auto dstColor = pieceToColor(pieceDst);
    auto dstType = pieceToType(pieceDst);

    const auto srcBB = indexToBB(src);
    const auto dstBB = indexToBB(dst);
    const auto toggleBB = srcBB | dstBB;

    // Undo hash: xóa castling & en passant hiện tại
    hash ^= zobrist::castling[state->castlingRights & 0xF];
    if (state->enPassant != SquareNone)
        hash ^= zobrist::enPassant[state->enPassant];
    hash ^= zobrist::side;

    // Undo hash: di chuyển quân ngược
    hash ^= zobrist::pieces[pieceDst][dst];
    // Nếu promotion, quân ở src là Pawn chứ không phải quân đã promote
    if (move.is<Move::Promotion>()) {
        Piece pawn = makePiece(dstColor, Pawn);
        hash ^= zobrist::pieces[pawn][src];
    } else {
        hash ^= zobrist::pieces[pieceDst][src];
    }

    // Undo hash: khôi phục quân bị bắt
    if (state->capturedPiece != PieceNone)
        hash ^= zobrist::pieces[state->capturedPiece][dst];

    colors[dstColor] ^= toggleBB;
    types[dstType] ^= toggleBB;
    pieces[src] = pieceDst;

    pieces[dst] = state->capturedPiece;
    if (state->capturedPiece != PieceNone){
        auto dstColor = pieceToColor(state->capturedPiece);
        auto dstType = pieceToType(state->capturedPiece);
        colors[dstColor] |= indexToBB(dst);
        types[dstType] |= indexToBB(dst);
    }

    if (move.is<Move::KingCastling>()){
        checkAndUndoMoveCastling<CastlingWK>(dst);
        checkAndUndoMoveCastling<CastlingBK>(dst);
    } else if (move.is<Move::QueenCastling>()){
        checkAndUndoMoveCastling<CastlingWQ>(dst);
        checkAndUndoMoveCastling<CastlingBQ>(dst);
    } else if (move.is<Move::Enpassant>()){
        int enemyPawn = dstColor == White ? dst - 8 : dst + 8;
        BB enemyPawnBB = indexToBB(enemyPawn);
        colors[!dstColor] |= enemyPawnBB;
        types[Pawn] |= enemyPawnBB;
        Piece epPiece = makePiece(!dstColor, Pawn);
        pieces[enemyPawn] = epPiece;
        hash ^= zobrist::pieces[epPiece][enemyPawn];
    } else if (move.is<Move::Promotion>()){
        pieces[src] = makePiece(dstColor, Pawn);
        types[dstType] ^= srcBB;
        types[Pawn] |= srcBB;
    }

    state = state->previous;
    side = !side;

    // Khôi phục castling & en passant cũ vào hash
    hash ^= zobrist::castling[state->castlingRights & 0xF];
    if (state->enPassant != SquareNone)
        hash ^= zobrist::enPassant[state->enPassant];

    return 0;
}

void Board::refresh() noexcept {
    auto myKing = colors[side] & types[King];
    auto myKingIdx = lsbIndex(myKing);
    auto occ = getOccupancy();
    auto enemies = colors[!side];
    state->checkers = getSqAttackTo(myKingIdx, occ) & enemies;
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
                str += std::to_string(0);

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



template<CastlingRights c>
void Board::checkAndDoMoveCastling(int dst) noexcept
{
    constexpr auto static testCasting = getCastlingIndex<c, King, false>();
    if (dst == testCasting){
        constexpr auto static toggle = getCastlingToggle<c, Rook>();
        constexpr auto static rookSrc = getCastlingIndex<c, Rook, true>();
        constexpr auto static rookDst = getCastlingIndex<c, Rook, false>();
        constexpr auto static color = castlingRightToColor<c>();
        constexpr auto static rookPiece = makePiece<color, Rook>();
        colors[color] ^= toggle;
        types[Rook] ^= toggle;
        pieces[rookSrc] = PieceNone;
        pieces[rookDst] = rookPiece;
        hash ^= zobrist::pieces[rookPiece][rookSrc];
        hash ^= zobrist::pieces[rookPiece][rookDst];
    }
}

template<CastlingRights c>
void Board::checkAndUndoMoveCastling(int dst) noexcept
{
    constexpr auto static testCasting = getCastlingIndex<c, King, false>();
    if (dst == testCasting){
        constexpr static auto toggle = getCastlingToggle<c, Rook>();
        constexpr static auto rookSrc = getCastlingIndex<c, Rook, true>();
        constexpr static auto rookDst = getCastlingIndex<c, Rook, false>();
        constexpr auto static color = castlingRightToColor<c>();
        constexpr auto static rookPiece = makePiece<color, Rook>();
        colors[color] ^= toggle;
        types[Rook] ^= toggle;
        pieces[rookSrc] = rookPiece;
        pieces[rookDst] = PieceNone;
        hash ^= zobrist::pieces[rookPiece][rookSrc];
        hash ^= zobrist::pieces[rookPiece][rookDst];
    }
}
