#pragma once
#include "board.h"
#include "define.h"

namespace kc {

inline int genMoveList(const Board &board, Move *moveList) noexcept;

template <Color color>
inline int genMoveList(const Board &board, Move *moveList) noexcept;

template <Color color>
inline int getMoveListWhenDoubleCheck(const Board &board, Move *moveList) noexcept;

std::vector<Move> getMoveListForSquare(const Board &board, Square square);

}
