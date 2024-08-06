#pragma once
#include <algorithm/chessboard.h>


bool parseFENString(const std::string &fen, kchess::ChessBoard *result);

std::string toFenString(const kchess::ChessBoard &chessBoard);
