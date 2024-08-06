#pragma once
#include "board.h"
#include "define.h"

namespace kchess {
void generateMoveList(const Board &board, Move *moveList, int &count);
int countMoveList(const Board &board, Color color);
std::vector<Move> getMoveListForSquare(const Board &board, Square square);

void generateMove(const Board &board);
}
