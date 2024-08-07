#include "util.h"
#include <vector>
using namespace kc;

std::string toFenString(const Board &board){
    std::string str;
    for (int rank = Rank_NB - 1; rank >= 0; rank--){
        int countEmpty = 0;
        for (int file = 0; file < File_NB; file++){
            int index = rank * 8 + file;
            auto bb = squareToBB(index);
            if (board.colors[Color::White] & bb){
                if (countEmpty > 0){
                    str += std::to_string(countEmpty);
                    countEmpty = 0;
                }

                if (board.types[PieceType::Pawn] & bb){
                    str += 'P';
                } else if (board.types[PieceType::Knight] & bb){
                    str += 'N';
                } else if (board.types[PieceType::Bishop] & bb){
                    str += 'B';
                } else if (board.types[PieceType::Rook] & bb){
                    str += 'R';
                } else if (board.types[PieceType::Queen] & bb){
                    str += 'Q';
                } else if (board.types[PieceType::King] & bb){
                    str += 'K';
                }
            } else if (board.colors[Color::Black] & bb){
                if (countEmpty > 0){
                    str += std::to_string(countEmpty);
                    countEmpty = 0;
                }
                if (board.types[PieceType::Pawn] & bb){
                    str += 'p';
                } else if (board.types[PieceType::Knight] & bb){
                    str += 'n';
                } else if (board.types[PieceType::Bishop] & bb){
                    str += 'b';
                } else if (board.types[PieceType::Rook] & bb){
                    str += 'r';
                } else if (board.types[PieceType::Queen] & bb){
                    str += 'q';
                } else if (board.types[PieceType::King] & bb){
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
    str += board.side == White ? "w" : "b";
    str += " ";
    if (board.whiteOO){
        str += "K";
    }
    if (board.whiteOOO){
        str += "Q";
    }
    if (board.blackOO){
        str += "k";
    }
    if (board.blackOOO){
        str += "q";
    }
    if (str.at(str.size() - 1) == ' '){
        str += "-";
    }
    str += " ";

    if (board.enPassant == SquareNone){
        str += "-";
    } else {
        auto r = getRank(board.enPassant);
        auto f = getFile(board.enPassant);

        str += fileToChar(f);
        str += rankToChar(r);
    }
    str += " ";
    str += std::to_string(board.halfMoveClock);
    str += " ";
    str += std::to_string(board.fullMoveNumber);

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


bool parseFENString(const std::string &fen, Board *result)
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
        auto file = charToFile(enPassant.at(0));
        auto rank = charToRank(enPassant.at(1));
        result->enPassant = makeSquare(rank, file);
    }

    result->halfMoveClock = std::stoi(halfMoveClock);
    result->fullMoveNumber = std::stoi(fullMoveNumber);

    return true;
}
