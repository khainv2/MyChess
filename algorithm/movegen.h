#pragma once
#include "board.h"
#include "define.h"

namespace kc {
int generateMoveList(const Board &board, Move *moveList);

template <Color color>
int genMoveList(const Board &board, Move *moveList);

std::vector<Move> getMoveListForSquare(const Board &board, Square square);

void testPerft();
int genMoveRecur(Board &board, int depth);

std::string testFenPerft();
}
