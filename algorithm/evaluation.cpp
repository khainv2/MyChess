#include "evaluation.h"
#include "movegenerator.h"

int kchess::eval::estimate(const ChessBoard &board)
{
    u64 b = board.blacks, w = board.whites;

    int materialValue =
              (popCount(board.pawns & w) - popCount(board.pawns & b)) * Value_Pawn
            + (popCount(board.knights & w) - popCount(board.knights & b)) * Value_Knight
            + (popCount(board.bishops & w) - popCount(board.bishops & b)) * Value_Bishop
            + (popCount(board.rooks & w) - popCount(board.rooks & b)) * Value_Rook
            + (popCount(board.queens & w) - popCount(board.queens & b)) * Value_Queen
            + (popCount(board.kings & w) - popCount(board.kings & b)) * Value_King;
    int mobilityValue = 10 * (countMoveList(board, White) - countMoveList(board, Black));
    return materialValue + mobilityValue;
}
