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

    int genMoveList(const Board &board, Move *moveList) noexcept;

    template <Color color, bool isJustCount>
    inline Move *genMoveList(const Board &board, Move *moveList) noexcept;

    template <Color color>
    inline Move *getMoveListForPawn(const Board &board, Move *moveList) noexcept;

    template <Color color>
    inline Move *getMoveListForKnight(const Board &board, Move *moveList) noexcept;

    template <Color color>
    inline Move *getMoveListForBishopQueen(const Board &board, Move *moveList) noexcept;

    template <Color color>
    inline Move *getMoveListForRookQueen(const Board &board, Move *moveList) noexcept;

    template <Color color>
    inline Move *getMoveListForKing(const Board &board, Move *moveList) noexcept;

    template <Color color, CastlingRights right>
    inline Move *getCastlingMoveList(const Board &board, Move *moveList) noexcept;

    template <Color color>
    inline Move *getMoveListWhenDoubleCheck(Move *moveList) noexcept;

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

    BB kingBan;
    BB checkMask;
    BB pinMaskDiagonal;
    BB pinMaskCross;
    BB targetForHeavyPiece;
};


}
