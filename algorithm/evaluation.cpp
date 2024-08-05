#include "evaluation.h"
#include "movegenerator.h"

int kchess::eval::estimate(const ChessBoard &board)
{
    u64 b = board.colors[Black], w = board.colors[White];

    int materialValue =
              (popCount(board.pieces[Pawn] & w) - popCount(board.pieces[Pawn] & b)) * Value_Pawn
            + (popCount(board.pieces[Knight] & w) - popCount(board.pieces[Knight] & b)) * Value_Knight
            + (popCount(board.pieces[Bishop] & w) - popCount(board.pieces[Bishop] & b)) * Value_Bishop
            + (popCount(board.pieces[Rook] & w) - popCount(board.pieces[Rook] & b)) * Value_Rook
            + (popCount(board.pieces[Queen] & w) - popCount(board.pieces[Queen] & b)) * Value_Queen
            + (popCount(board.pieces[King] & w) - popCount(board.pieces[King] & b)) * Value_King;

    
    int mobilityValue = 10 * (countMoveList(board, White) - countMoveList(board, Black));
    return materialValue + mobilityValue;
}
