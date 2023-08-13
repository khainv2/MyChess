#include "util.h"

#include <QStringList>

using namespace kchess;
enum FenField {
    PieceData,
    ActiveColor,
    CastlingAvailability,
    EnPassantTargetSquare,
    HalfMoveClock,
    FullMoveNumber,
    FenField_NB,
};

constexpr Piece charToPiece(char c){
    switch (c){
    case 'p': case 'P': return Pawn;
    case 'r': case 'R': return Rook;
    case 'n': case 'N': return Knight;
    case 'b': case 'B': return Bishop;
    case 'k': case 'K': return King;
    case 'q': case 'Q': return Queen;
    default: return Piece_NB;
    }
}

constexpr Color charToColor(char c){
    return c < 'a' ? White : Black;
}

bool parseFENString(const QString &fen, ChessBoard *result)
{
    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
    // rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1

    QStringList words = fen.split(' ');
    if (words.size() != FenField_NB)
        return false;
    // Parse piece
    auto pieceData = words.at(PieceData);
    auto pieceRowList = pieceData.split('/');
    if (pieceRowList.size() != Rank_NB){
        return false;
    }
    for (int i = 0; i < Rank_NB; i++){
        auto pieceRow = pieceRowList.at(i);
        int f = 0;
        for (const auto &p: pieceRow){
            if (p.isDigit()){
                f += p.digitValue();
            } else {
                auto c = p.toLatin1();
                auto piece = charToPiece(c);
                auto color = charToColor(c);
                int rank = 7 - i;
                int file = f;
                int index = rank * 8 + file;
                if (color == White) result->whites += (1LL << index);
                if (color == Black) result->blacks += (1LL << index);

                if (piece == Pawn) result->pawns += (1LL << index);
                if (piece == Rook) result->rooks += (1LL << index);
                if (piece == Knight) result->knights += (1LL << index);
                if (piece == Bishop) result->bishops += (1LL << index);
                if (piece == Queen) result->queens += (1LL << index);
                if (piece == King) result->kings += (1LL << index);
                f++;
            }
        }
    }

    auto activeColor = words.at(ActiveColor);
    auto castlingAvailability = words.at(CastlingAvailability);
    auto enPassantTargetSquare = words.at(EnPassantTargetSquare);
    auto halfMoveClock = words.at(HalfMoveClock);
    auto fullMoveNumber = words.at(FullMoveNumber);


    return true;
}
