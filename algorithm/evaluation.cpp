#include "evaluation.h"
#include "movegen.h"

int kc::eval::estimate(const Board &board)
{
//    u64 b = board.colors[Black], w = board.colors[White];
    u64 b = board.colors[Black], w = board.colors[White];
    int material = (popCount(board.types[Pawn] & w) - popCount(board.types[Pawn] & b)) * Value_Pawn
            + (popCount(board.types[Knight] & w) - popCount(board.types[Knight] & b)) * Value_Knight
            + (popCount(board.types[Bishop] & w) - popCount(board.types[Bishop] & b)) * Value_Bishop
            + (popCount(board.types[Rook] & w) - popCount(board.types[Rook] & b)) * Value_Rook
            + (popCount(board.types[Queen] & w) - popCount(board.types[Queen] & b)) * Value_Queen;

    int mobility = 10 * (MoveGen::instance->countMoveList<White>(board)
                              - MoveGen::instance->countMoveList<Black>(board));
    auto wp = board.types[Pawn] & w;
    auto bp = board.types[Pawn] & b;
    // Count isolated pawn



    return material + mobility;
}

int kc::eval::getMaterial(const Board &board) {
    u64 b = board.colors[Black], w = board.colors[White];
    return (popCount(board.types[Pawn] & w) - popCount(board.types[Pawn] & b)) * Value_Pawn
            + (popCount(board.types[Knight] & w) - popCount(board.types[Knight] & b)) * Value_Knight
            + (popCount(board.types[Bishop] & w) - popCount(board.types[Bishop] & b)) * Value_Bishop
            + (popCount(board.types[Rook] & w) - popCount(board.types[Rook] & b)) * Value_Rook
            + (popCount(board.types[Queen] & w) - popCount(board.types[Queen] & b)) * Value_Queen;
}
