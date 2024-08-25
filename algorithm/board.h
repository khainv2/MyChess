#pragma once
#include "define.h"
#include <QDebug>
namespace kc {
struct BoardState {
    int castlingRights;
    // En passant target square
    Square enPassant;
    // Half move clock
    int halfMoveClock;
    // Full move number
    int fullMoveNumber;

    // Captured piece
    Piece capturedPiece;
    BoardState *previous = nullptr;

    BoardState() = default;
    BoardState(const BoardState &state) = default;

    BoardState(CastlingRights castlingRights, Square enPassant, int halfMoveClock, int fullMoveNumber, Piece capturedPiece, BoardState *previous)
        : castlingRights(castlingRights), enPassant(enPassant), halfMoveClock(halfMoveClock), fullMoveNumber(fullMoveNumber), capturedPiece(capturedPiece), previous(previous) {}

    BoardState &operator=(const BoardState &state) = default;

    template<Color color>
    constexpr inline int enPassantTarget() const noexcept {
        return color == White ? enPassant + 8 : enPassant - 8;
    }

    void print() const {
        qDebug() << "Castling rights: " << castlingRights;
        qDebug() << "En passant: " << enPassant;
        qDebug() << "Half move clock: " << halfMoveClock;
        qDebug() << "Full move number: " << fullMoveNumber;
        qDebug() << "Captured piece: " << pieceToChar(capturedPiece);
    }
};

struct Board {
    Piece pieces[Square_Count] = { PieceNone };
    BB colors[Color_NB] = {0};
    BB types[PieceType_NB] = {0};

    Color side = White;
    BoardState *state = nullptr;

    Board() noexcept ;
    ~Board() noexcept ;

    template<Color color>
    constexpr inline BB getPieceBB() const noexcept {
        return colors[color];
    }
    template<Color color, PieceType piece>
    constexpr inline BB getPieceBB() const noexcept {
        return colors[color] & types[piece];
    }
    template<Color color, PieceType piece, PieceType piece2>
    constexpr inline BB getPieceBB() const noexcept {
        return colors[color] & (types[piece] | types[piece2]);
    }
    template<PieceType piece, PieceType piece2>
    constexpr inline BB getPieceBB() const noexcept {
        return types[piece] | types[piece2];
    }
    template<PieceType piece>
    constexpr inline BB getPieceBB() const noexcept {
        return types[piece];
    }

    template <Color color>
    constexpr inline BB getMines() const noexcept {
        return colors[color];
    }

    template <Color color>
    constexpr inline BB getEnemies() const noexcept {
        return colors[!color];
    }

    constexpr inline BB getOccupancy() const noexcept {
        return colors[Black] | colors[White];
    }

    template<Color mine>
    inline BB getKingBan() const noexcept;

    BB getSqAttackTo(int sq, BB occ) const noexcept ;

    int doMove(Move move, BoardState &state) noexcept ;
    int undoMove(Move move) noexcept;

    std::string getPrintable(int tab = 0) const;

};
}
