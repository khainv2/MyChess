#pragma once
#include "board.h"
#include "define.h"

namespace kc {
int genMoveList(const Board &board, Move *moveList);

template <Color color>
int genMoveList(const Board &board, Move *moveList);



std::vector<Move> getMoveListForSquare(const Board &board, Square square);

}
