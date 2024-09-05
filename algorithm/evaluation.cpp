#include "evaluation.h"
#include "movegen.h"

int kc::eval::estimate(const Board &board)
{
    u64 b = board.colors[Black], w = board.colors[White];

    int materialValue = getMaterial(board);

//    int mobilityValue = 10 * (MoveGen::instance->countMoveList<White>(board)
//                              - MoveGen::instance->countMoveList<Black>(board));
//    materialValue += mobilityValue;
    return materialValue;
}

int kc::eval::getMaterial(const Board &board) {
    u64 b = board.colors[Black], w = board.colors[White];
    return (popCount(board.types[Pawn] & w) - popCount(board.types[Pawn] & b)) * Value_Pawn
            + (popCount(board.types[Knight] & w) - popCount(board.types[Knight] & b)) * Value_Knight
            + (popCount(board.types[Bishop] & w) - popCount(board.types[Bishop] & b)) * Value_Bishop
            + (popCount(board.types[Rook] & w) - popCount(board.types[Rook] & b)) * Value_Rook
            + (popCount(board.types[Queen] & w) - popCount(board.types[Queen] & b)) * Value_Queen;
}
