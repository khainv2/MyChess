#pragma once
#include "chessboard.h"
#include "define.h"

namespace kchess {
void generateMoveList(const ChessBoard &board, Move *moveList, int &count);
int countMoveList(const ChessBoard &board, Color color);
Bitboard getMobility(const ChessBoard &board, Square square);

}
