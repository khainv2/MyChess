#pragma once
#include "board.h"
#include "define.h"

namespace kc {

class MoveGen {
public:
    static MoveGen *instance;
    static void init();

    int genMoveList(const Board &board, Move *moveList) noexcept;
    int countMoveList(const Board &board) noexcept;

    std::vector<Move> getMoveListForSquare(const Board &board, Square square);
public:
    MoveGen();
    inline BB getPinMaskDiagonal() const noexcept;
    inline BB getPinMaskCross() const noexcept;

    template <Color mine>
    inline void prepare(const Board &board) noexcept;

    template <Color mine>
    inline void updateCheckMaskAndPin() noexcept;

    template <Color color>
    inline Move *genMoveList(const Board &board, Move *moveList) noexcept;
    template <Color color>
    inline int countMoveList(const Board &board) noexcept;

    template <Color color>
    inline Move *getMoveListForPawn(const Board &board, Move *moveList) noexcept;
    template <Color color>
    inline int countPawnMove(const Board &board) noexcept;

    template <Color color>
    inline Move *getMoveListForKnight(const Board &board, Move *moveList) noexcept;
    template <Color color>
    inline int countKnightMove(const Board &board) noexcept;

    template <Color color>
    inline Move *getMoveListForBishopQueen(const Board &board, Move *moveList) noexcept;
    template <Color color>
    inline int countBishopQueenMove(const Board &board) noexcept;

    // Lấy nước đi của xe + các nước đi ngang dọc của hậu
    template <Color color>
    inline Move *getMoveListForRookQueen(const Board &board, Move *moveList) noexcept;
    template <Color color>
    inline int countRookQueenMove(const Board &board) noexcept;

    // Lấy nước đi của vua (trong trường hợp bình thường không bị chiếu)
    template <Color color, bool isDoubleCheck>
    inline Move *getMoveListForKing([[maybe_unused]] const Board &board, Move *moveList) noexcept;
    template <Color color, bool isDoubleCheck>
    inline int countKingMove([[maybe_unused]] const Board &board) noexcept;

    // Lấy nước đi nhập thành
    template <Color color, CastlingRights right>
    inline Move *getCastlingMoveList(const Board &board, Move *moveList) noexcept;
    template <Color color, CastlingRights right>
    inline int countCastlingMove(const Board &board) noexcept;

    // Kiểm tra tất cả các ô mà vua không được phép đi tới
    template<Color mine>
    inline BB getKingBan(const Board &board) noexcept;

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
    BB checkers;
    BB checkMask;
    BB pinMaskDiagonal;
    BB pinMaskCross;
    BB targetForHeavyPiece;
};


}
