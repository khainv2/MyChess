#include "evaluation.h"
#include "movegen.h"

int kc::eval::estimate(const Board &board)
{
    u64 b = board.colors[Black], w = board.colors[White];

    int materialValue =
              (popCount(board.types[Pawn] & w) - popCount(board.types[Pawn] & b)) * Value_Pawn
            + (popCount(board.types[Knight] & w) - popCount(board.types[Knight] & b)) * Value_Knight
            + (popCount(board.types[Bishop] & w) - popCount(board.types[Bishop] & b)) * Value_Bishop
            + (popCount(board.types[Rook] & w) - popCount(board.types[Rook] & b)) * Value_Rook
            + (popCount(board.types[Queen] & w) - popCount(board.types[Queen] & b)) * Value_Queen
            + (popCount(board.types[King] & w) - popCount(board.types[King] & b)) * Value_King;

//    int mobilityValue = 10 * (countMoveList(board, White) - countMoveList(board, Black));
//    materialValue += mobilityValue;
    return materialValue;
}
