#pragma once
#include "board.h"
#include "define.h"

namespace kc {
int generateMoveList(const Board &board, Move *moveList);

template <Color color>
int genMoveList(const Board &board, Move *moveList);

int countMoveList(const Board &board, Color color);
std::vector<Move> getMoveListForSquare(const Board &board, Square square);

void testPerft();
int genMoveRecur(const Board &board, int depth);
}
