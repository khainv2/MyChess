#include "util.h"
#include <vector>
#include "evaluation.h"
using namespace kc;

std::string toFenString(const Board &board){
    std::string str;
    for (int rank = Rank_NB - 1; rank >= 0; rank--){
        int countEmpty = 0;
        for (int file = 0; file < File_NB; file++){
            int index = rank * 8 + file;
            auto bb = indexToBB(index);
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

    auto state = board.state;

    str += " ";
    str += board.side == White ? "w" : "b";
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

    if (fen.empty()){
        qDebug( )<< "fen empty";
        return false;
    }
    
    auto segments = split(fen, " ");
    if (segments.size() != 6){
        qDebug() << "error num of segment not valid";
        return false;
    }

    // Parse piece
    auto pieceData = segments.at(0);
    auto pieceRowList = split(pieceData, "/");
    if (pieceRowList.size() != Rank_NB){
        qDebug() << "Num rank not valid";
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

    if (result->state != nullptr){
        delete result->state;
    }

    result->state = new BoardState();
    result->state->castlingRights = NoCastling;

    if (castlingAvailability.find("K") != std::string::npos){
        result->state->castlingRights |= CastlingWK;
    }
    if (castlingAvailability.find("Q") != std::string::npos){
        result->state->castlingRights |= CastlingWQ;
    }
    if (castlingAvailability.find("k") != std::string::npos){
        result->state->castlingRights |= CastlingBK;
    }
    if (castlingAvailability.find("q") != std::string::npos){
        result->state->castlingRights |= CastlingBQ;
    }
    
    if (enPassant == "-"){
        result->state->enPassant = SquareNone;
    } else {
        auto file = charToFile(enPassant.at(0));
        auto rank = charToRank(enPassant.at(1));
        result->state->enPassant = makeSquare(rank, file);
    }
    result->state->halfMoveClock = std::stoi(halfMoveClock);
    result->refresh();
    result->material = eval::getMaterial(*result);
    //    result->state->fullMoveNumber = std::stoi(fullMoveNumber);
    return true;
}
