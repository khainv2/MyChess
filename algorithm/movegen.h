#pragma once
#include "board.h"
#include "define.h"

namespace kc {
void generateMoveList(const Board &board, Move *moveList, int &count);
int countMoveList(const Board &board, Color color);
std::vector<Move> getMoveListForSquare(const Board &board, Square square);

void generateMove(const Board &board);
void genMoveRecur(const Board &board, Move *ptr, int &totalCount, int depth);
}
