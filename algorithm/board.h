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
    constexpr inline int enPassantTarget() const {
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

    Board();
    ~Board();

    template<Color color>
    constexpr inline BB getPieceBB() const {
        return colors[color];
    }
    template<Color color, PieceType piece>
    constexpr inline BB getPieceBB() const {
        return colors[color] & types[piece];
    }
    template<Color color, PieceType piece, PieceType piece2>
    constexpr inline BB getPieceBB() const {
        return colors[color] & (types[piece] | types[piece2]);
    }
    template<PieceType piece, PieceType piece2>
    constexpr inline BB getPieceBB() const {
        return types[piece] | types[piece2];
    }
    template<PieceType piece>
    constexpr inline BB getPieceBB() const {
        return types[piece];
    }

    template <Color color>
    constexpr inline BB getMines() const {
        return colors[color];
    }

    template <Color color>
    constexpr inline BB getEnemies() const {
        return colors[!color];
    }

    constexpr inline BB getOccupancy() const {
        return colors[Black] | colors[White];
    }

    BB getSqAttackTo(int sq, BB occ) const;

    int doMove(Move move, BoardState &state);
    int undoMove(Move move);

    std::string getPrintable(int tab = 0) const;

    void compareTo(const Board &other) const {
        for (int i = 0; i < Square_Count; i++) {
            if (pieces[i] != other.pieces[i]) {
                qDebug() << "Mismatch at square " << i << " " << pieceToChar(pieces[i]) << " != " << pieceToChar(other.pieces[i]);
            }
        }

        for (int i = 0; i < Color_NB; i++) {
            if (colors[i] != other.colors[i]) {
                qDebug() << "Mismatch at color " << i << " " << colors[i] << " != " << other.colors[i];
            }
        }

        for (int i = 0; i < PieceType_NB; i++) {
            if (types[i] != other.types[i]) {
                qDebug() << "Mismatch at type " << i << " " << types[i] << " != " << other.types[i];
            }
        }

        if (side != other.side) {
            qDebug() << "Mismatch at side " << side << " != " << other.side;
        }

        if (state->castlingRights != other.state->castlingRights) {
            qDebug() << "Mismatch at castling rights " << state->castlingRights << " != " << other.state->castlingRights;
        }

        if (state->enPassant != other.state->enPassant) {
            qDebug() << "Mismatch at en passant " << state->enPassant << " != " << other.state->enPassant;
        }

        if (state->halfMoveClock != other.state->halfMoveClock) {
            qDebug() << "Mismatch at half move clock " << state->halfMoveClock << " != " << other.state->halfMoveClock;
        }

        if (state->fullMoveNumber != other.state->fullMoveNumber) {
            qDebug() << "Mismatch at full move number " << state->fullMoveNumber << " != " << other.state->fullMoveNumber;
        }

        if (state->capturedPiece != other.state->capturedPiece) {
            qDebug() << "Mismatch at captured piece " << pieceToChar(state->capturedPiece) << " != " << pieceToChar(other.state->capturedPiece);
        }

        // if (state->previous != other.state->previous) {
        //     qDebug() << "Mismatch at previous state " << state->previous << " != " << other.state->previous;
        // }


    }

};
}
