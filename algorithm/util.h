#pragma once
#include <algorithm/board.h>


bool parseFENString(const std::string &fen, kchess::Board *result);

std::string toFenString(const kchess::Board &chessBoard);
