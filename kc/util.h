#pragma once
#include <kc/board.h>


bool parseFENString(const std::string &fen, kc::Board *result);

std::string toFenString(const kc::Board &chessBoard);
