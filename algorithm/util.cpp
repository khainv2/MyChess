#include "util.h"
#include <vector>
using namespace kchess;

std::string toFenString(const ChessBoard &chessBoard){
    std::string str;
    for (int rank = Rank_NB - 1; rank >= 0; rank--){
        int countEmpty = 0;
        for (int file = 0; file < File_NB; file++){
            int index = rank * 8 + file;
            auto bb = squareToBB(index);
            if (chessBoard.colors[Color::White] & bb){
                if (countEmpty > 0){
                    str += std::to_string(countEmpty);
                    countEmpty = 0;
                }

                if (chessBoard.types[PieceType::Pawn] & bb){
                    str += 'P';
                } else if (chessBoard.types[PieceType::Knight] & bb){
                    str += 'N';
                } else if (chessBoard.types[PieceType::Bishop] & bb){
                    str += 'B';
                } else if (chessBoard.types[PieceType::Rook] & bb){
                    str += 'R';
                } else if (chessBoard.types[PieceType::Queen] & bb){
                    str += 'Q';
                } else if (chessBoard.types[PieceType::King] & bb){
                    str += 'K';
                }
            } else if (chessBoard.colors[Color::Black] & bb){
                if (countEmpty > 0){
                    str += std::to_string(countEmpty);
                    countEmpty = 0;
                }
                if (chessBoard.types[PieceType::Pawn] & bb){
                    str += 'p';
                } else if (chessBoard.types[PieceType::Knight] & bb){
                    str += 'n';
                } else if (chessBoard.types[PieceType::Bishop] & bb){
                    str += 'b';
                } else if (chessBoard.types[PieceType::Rook] & bb){
                    str += 'r';
                } else if (chessBoard.types[PieceType::Queen] & bb){
                    str += 'q';
                } else if (chessBoard.types[PieceType::King] & bb){
                    str += 'k';
                }
            } else {
                countEmpty++;
                if (file == File_NB - 1){
                    str += std::to_string(countEmpty);
                }
            }

            if (file == File_NB - 1 && rank != 0){
                str += "/";
            }
        }
    }
    // return str + " w KQkq - 0 1";

    str += " ";
    str += chessBoard.side == White ? "w" : "b";
    str += " ";
    if (chessBoard.whiteOO){
        str += "K";
    }
    if (chessBoard.whiteOOO){
        str += "Q";
    }
    if (chessBoard.blackOO){
        str += "k";
    }
    if (chessBoard.blackOOO){
        str += "q";
    }
    if (str.at(str.size() - 1) == ' '){
        str += "-";
    }
    str += " ";

    if (chessBoard.enPassant == SquareNone){
        str += "-";
    } else {
        int idx = bbToSquare(chessBoard.enPassant);
        str += 'a' + (idx % 8);
        str += '8' - (idx / 8);
    }
    str += " ";
    str += std::to_string(chessBoard.halfMoveClock);
    str += " ";
    str += std::to_string(chessBoard.fullMoveNumber);

    return str;

}


std::vector<std::string> split(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr (pos_start));
    return res;
}


bool parseFENString(const std::string &fen, ChessBoard *result)
{
    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
    // rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1

    if (fen.empty())
        return false;
    
    auto segments = split(fen, " ");
    if (segments.size() != 6){
        return false;
    }

    // Parse piece
    auto pieceData = segments.at(0);
    auto pieceRowList = split(pieceData, "/");
    if (pieceRowList.size() != Rank_NB){
        return false;
    }

    for (int i = 0; i < Rank_NB; i++){
        auto pieceRow = pieceRowList.at(i);
        int f = 0;
        for (const auto &p: pieceRow){
            if (p >= '0' && p <= '9'){
                f += p - '0';
            } else {
                auto piece = charToPiece(p);
                int rank = 7 - i;
                int file = f;
                int index = rank * 8 + file;
                
                if (piece == WhitePawn){
                    result->colors[Color::White] |= (1LL << index);
                    result->types[Pawn] |= (1LL << index);
                } else if (piece == WhiteRook){
                    result->colors[Color::White] |= (1LL << index);
                    result->types[Rook] |= (1LL << index);
                } else if (piece == WhiteKnight){
                    result->colors[Color::White] |= (1LL << index);
                    result->types[Knight] |= (1LL << index);
                } else if (piece == WhiteBishop){
                    result->colors[Color::White] |= (1LL << index);
                    result->types[Bishop] |= (1LL << index);
                } else if (piece == WhiteQueen){
                    result->colors[Color::White] |= (1LL << index);
                    result->types[Queen] |= (1LL << index);
                } else if (piece == WhiteKing){
                    result->colors[Color::White] |= (1LL << index);
                    result->types[King] |= (1LL << index);
                } else if (piece == BlackPawn){
                    result->colors[Color::Black] |= (1LL << index);
                    result->types[Pawn] |= (1LL << index);
                } else if (piece == BlackRook){
                    result->colors[Color::Black] |= (1LL << index);
                    result->types[Rook] |= (1LL << index);
                } else if (piece == BlackKnight){
                    result->colors[Color::Black] |= (1LL << index);
                    result->types[Knight] |= (1LL << index);
                } else if (piece == BlackBishop){
                    result->colors[Color::Black] |= (1LL << index);
                    result->types[Bishop] |= (1LL << index);
                } else if (piece == BlackQueen){
                    result->colors[Color::Black] |= (1LL << index);
                    result->types[Queen] |= (1LL << index);
                } else if (piece == BlackKing){
                    result->colors[Color::Black] |= (1LL << index);
                    result->types[King] |= (1LL << index);
                }
                result->pieces[index] = piece;
                
                f++;
            }
        }
    }

    std::string activeColor = segments.at(1);
    std::string castlingAvailability = segments.at(2);
    std::string enPassant = segments.at(3);
    std::string halfMoveClock = segments.at(4);
    std::string fullMoveNumber = segments.at(5);

    if (activeColor == "w"){
        result->side = White;
    } else {
        result->side = Black;
    }

    if (castlingAvailability.find("K") != std::string::npos){
        result->whiteOO = true;
    }
    if (castlingAvailability.find("Q") != std::string::npos){
        result->whiteOOO = true;
    }
    if (castlingAvailability.find("k") != std::string::npos){
        result->blackOO = true;
    }
    if (castlingAvailability.find("q") != std::string::npos){
        result->blackOOO = true;
    }

    if (enPassant == "-"){
        result->enPassant = SquareNone;
    } else {
        auto rank = Rank(8 - (enPassant.at(1) - '0'));
        auto file = File(enPassant.at(0) - 'a');
        result->enPassant = makeSquare(rank, file);
    }

    result->halfMoveClock = std::stoi(halfMoveClock);
    result->fullMoveNumber = std::stoi(fullMoveNumber);

    return true;
}
