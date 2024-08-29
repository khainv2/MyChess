#pragma once
#include "board.h"
#include "define.h"

namespace kc {

class MoveGen {
public:
    static MoveGen *instance;
    static void init();
    MoveGen();

    inline BB getPinMaskDiagonal() const noexcept;
    inline BB getPinMaskCross() const noexcept;

    inline int genMoveList(const Board &board, Move *moveList) noexcept;


    template <Color color>
    inline Move *genMoveList(const Board &board, Move *moveList) noexcept;

    template <Color color>
    inline Move *getMoveListForPawn(const Board &board, Move *moveList) noexcept;

    template <Color color, PieceType piece>
    inline Move *getMoveListForPiece(const Board &board, Move *moveList) noexcept;


    template <Color color>
    inline Move *getMoveListWhenDoubleCheck(const Board &board, Move *moveList) noexcept;

    template<Color mine>
    inline BB getKingBan(const Board &board) noexcept;

    std::vector<Move> getMoveListForSquare(const Board &board, Square square);

private:
    BB occ, notOcc;
    BB myKing;
    int myKingIdx;
    BB mines;
    BB enemies;
    BB notMines;
    BB _enemyRookQueens;
    BB _enemyBishopQueens;

    BB checkMask;
    BB pinMaskDiagonal;
    BB pinMaskCross;
    BB target;
};


}
