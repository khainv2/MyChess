#include "define.h"
#include "attack.h"
#include <QDebug>

using namespace kc;


std::string kc::Move::getDescription() const {
    auto s = src();
    auto d = dst();
    static std::string fileTxt[8] = { "A", "B", "C", "D", "E", "F", "G", "H" };
    static std::string rankTxt[8] = { "1", "2", "3", "4", "5", "6", "7", "8" };

    int srcFileIdx = s % 8;
    int srcRankIdx = s / 8;
    int dstFileIdx = d % 8;
    int dstRankIdx = d / 8;
    if (type() == Promotion){
        auto p = getPromotionPieceType();
        return fileTxt[srcFileIdx] + rankTxt[srcRankIdx]
             + ">" + fileTxt[dstFileIdx] + rankTxt[dstRankIdx]
             + "[P]" + pieceToChar(makePiece(White, PieceType(p)));
    }
    return fileTxt[srcFileIdx] + rankTxt[srcRankIdx]
         + ">" + fileTxt[dstFileIdx] + rankTxt[dstRankIdx];
}

char kc::pieceToChar(Piece piece){
    switch (piece){
    case WhitePawn: return 'P';
    case WhiteRook: return 'R';
    case WhiteKnight: return 'N';
    case WhiteBishop: return 'B';
    case WhiteKing: return 'K';
    case WhiteQueen: return 'Q';
    case BlackPawn: return 'p';
    case BlackRook: return 'r';
    case BlackKnight: return 'n';
    case BlackBishop: return 'b';
    case BlackKing: return 'k';
    case BlackQueen: return 'q';
    default: return '.';
    }
}

Piece kc::charToPiece(char c){
    switch (c){
    case 'P': return WhitePawn;
    case 'R': return WhiteRook;
    case 'N': return WhiteKnight;
    case 'B': return WhiteBishop;
    case 'K': return WhiteKing;
    case 'Q': return WhiteQueen;
    case 'p': return BlackPawn;
    case 'r': return BlackRook;
    case 'n': return BlackKnight;
    case 'b': return BlackBishop;
    case 'k': return BlackKing;
    case 'q': return BlackQueen;
    default: return PieceNone;
    }

}
char kc::rankToChar(Rank r)
{
    return '1' + r;
}

Rank kc::charToRank(char c)
{
    return Rank(c - '1');
}

char kc::fileToChar(File f)
{
    return 'a' + f;
}

File kc::charToFile(char c)
{
    return File(c - 'a');
}
