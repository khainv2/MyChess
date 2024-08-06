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

QString toFenString(const ChessBoard &chessBoard){
    QString str;
    for (int rank = Rank_NB - 1; rank >= 0; rank--){
        int countEmpty = 0;
        for (int file = 0; file < File_NB; file++){
            int index = rank * 8 + file;
            auto bb = squareToBB(index);
            if (chessBoard.colors[Color::White] & bb){
                if (countEmpty > 0){
                    str += QString::number(countEmpty);
                    countEmpty = 0;
                }

                if (chessBoard.pieces[Piece::Pawn] & bb){
                    str += 'P';
                } else if (chessBoard.pieces[Piece::Knight] & bb){
                    str += 'N';
                } else if (chessBoard.pieces[Piece::Bishop] & bb){
                    str += 'B';
                } else if (chessBoard.pieces[Piece::Rook] & bb){
                    str += 'R';
                } else if (chessBoard.pieces[Piece::Queen] & bb){
                    str += 'Q';
                } else if (chessBoard.pieces[Piece::King] & bb){
                    str += 'K';
                }
            } else if (chessBoard.colors[Color::Black] & bb){
                if (countEmpty > 0){
                    str += QString::number(countEmpty);
                    countEmpty = 0;
                }
                if (chessBoard.pieces[Piece::Pawn] & bb){
                    str += 'p';
                } else if (chessBoard.pieces[Piece::Knight] & bb){
                    str += 'n';
                } else if (chessBoard.pieces[Piece::Bishop] & bb){
                    str += 'b';
                } else if (chessBoard.pieces[Piece::Rook] & bb){
                    str += 'r';
                } else if (chessBoard.pieces[Piece::Queen] & bb){
                    str += 'q';
                } else if (chessBoard.pieces[Piece::King] & bb){
                    str += 'k';
                }
            } else {
                countEmpty++;
                if (file == File_NB - 1){
                    str += QString::number(countEmpty);
                }
            }

            if (file == File_NB - 1 && rank != 0){
                str += "/";
            }
        }
    }
    // return str + " w KQkq - 0 1";

    str += " ";
    str += chessBoard.state.getActiveColor() == White ? "w" : "b";
    str += " ";
    if (chessBoard.state.getWhiteOO()){
        str += "K";
    }
    if (chessBoard.state.getWhiteOOO()){
        str += "Q";
    }
    if (chessBoard.state.getBlackOO()){
        str += "k";
    }
    if (chessBoard.state.getBlackOOO()){
        str += "q";
    }
    if (str.at(str.size() - 1) == ' '){
        str += "-";
    }
    str += " ";

    if (chessBoard.state.getEnpassantSquare() == Square(-1)){
        str += "-";
    } else {
        int idx = chessBoard.state.getEnpassantSquare().getIndex();
        str += QChar('a' + (idx % 8));
        str += QChar('8' - (idx / 8));
    }
    str += " ";
    str += QString::number(chessBoard.state.getCountHalfMove());

    return str;

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
                if (color == White) result->colors[Color::White] |= (1LL << index);
                if (color == Black) result->colors[Color::Black] |= (1LL << index);

                if (piece == Pawn) result->pieces[Pawn] |= (1LL << index);
                if (piece == Rook) result->pieces[Rook] |= (1LL << index);
                if (piece == Knight) result->pieces[Knight] |= (1LL << index);
                if (piece == Bishop) result->pieces[Bishop] |= (1LL << index);
                if (piece == Queen) result->pieces[Queen] |= (1LL << index);
                if (piece == King) result->pieces[King] |= (1LL << index);
                
                f++;
            }
        }
    }

    auto activeColor = words.at(ActiveColor);
    auto castlingAvailability = words.at(CastlingAvailability);
    auto enPassantTargetSquare = words.at(EnPassantTargetSquare);
    auto halfMoveClock = words.at(HalfMoveClock);
    auto fullMoveNumber = words.at(FullMoveNumber);


    if (activeColor == "w"){
        result->state.setActive(White);
    } else {
        result->state.setActive(Black);
    }

    if (castlingAvailability.contains('K')){
        result->state.setWhiteOO(true);
    }
    if (castlingAvailability.contains('Q')){
        result->state.setWhiteOOO(true);
    }
    if (castlingAvailability.contains('k')){
        result->state.setBlackOO(true);
    }
    if (castlingAvailability.contains('q')){
        result->state.setBlackOOO(true);
    }

    if (enPassantTargetSquare != "-"){
        int file = enPassantTargetSquare.at(0).toLatin1() - 'a';
        int rank = 8 - enPassantTargetSquare.at(1).digitValue();
        int index = rank * 8 + file;
        result->state.setEnpassantSquare(Square(index));
    }

    result->state.setCountHalfMove(halfMoveClock.toInt());
    return true;
}
