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
    inline int genMoveList(const Board &board, Move *moveList) noexcept;

    template <Color color>
    inline int getMoveListWhenDoubleCheck(const Board &board, Move *moveList) noexcept;

    template<Color mine>
    inline BB getKingBan(const Board &board) noexcept;

    std::vector<Move> getMoveListForSquare(const Board &board, Square square);

private:
    BB occ;
    BB myKing;
    int myKingIdx;
    BB mines;
    BB notMines;
    BB _enemyRookQueens;
    BB _enemyBishopQueens;

};


}
