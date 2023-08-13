#pragma once
#include "define.h"

namespace kchess {
void generateMoveList(const ChessBoard &board, Move *moveList, int &count);

Bitboard getMobility(const ChessBoard &board, Square square);

}
