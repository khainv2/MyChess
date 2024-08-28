#include "movegen.h"
#include "attack.h"
#include <bitset>
#include <QDebug>
#include <QElapsedTimer>
#include "evaluation.h"
#include "util.h"

using namespace kc;


MoveGen *MoveGen::instance = nullptr;


void MoveGen::init()
{
    instance = new MoveGen;
}

MoveGen::MoveGen(){}

BB MoveGen::getPinMaskDiagonal() const noexcept
{
    BB pinMaskDiagonal = 0;
    BB enemyBishopQueenHasXRayOnKing = attack::getBishopXRay(myKingIdx, occ) & _enemyBishopQueens;
    while (enemyBishopQueenHasXRayOnKing) {
        pinMaskDiagonal |= attack::between[myKingIdx][popLsb(enemyBishopQueenHasXRayOnKing)];
    }
    return pinMaskDiagonal;
}

BB MoveGen::getPinMaskCross() const noexcept
{
    BB pinMaskCross = 0;
    BB enemyRookQueenHasXRayOnKing = attack::getRookXRay(myKingIdx, occ) & _enemyRookQueens;
    while (enemyRookQueenHasXRayOnKing) {
        pinMaskCross |= attack::between[myKingIdx][popLsb(enemyRookQueenHasXRayOnKing)];
    }
    return pinMaskCross;
}

//template
int MoveGen::genMoveList(const Board &board, Move *moveList) noexcept
{
    if (board.side == White){
        return genMoveList<White>(board, moveList);
    } else {
        return genMoveList<Black>(board, moveList);
    }
}

template<Color mine>
int MoveGen::genMoveList(const Board &board, Move *moveList) noexcept
{
    myKing = board.getPieceBB<mine, King>();
    myKingIdx = lsbIndex(myKing);

    occ = board.getOccupancy();
    mines = board.getMines<mine>();
    notMines = ~mines;
    const BB enemies = board.getEnemies<mine>();

    // Tìm kiếm các ô đang tấn công vua
    const BB checkers = board.getSqAttackTo(myKingIdx, occ) & enemies;

    // Trong trường hợp chiếu đôi, tạo luôn movelist chỉ cho phép vua di chuyển
    if (isMoreThanOne(checkers)) {
        return getMoveListWhenDoubleCheck<mine>(board, moveList);
    }

    constexpr auto enemyColor = !mine;
    const BB notOcc = ~occ;
    _enemyRookQueens = board.getPieceBB<enemyColor, Rook, Queen>();
    _enemyBishopQueens = board.getPieceBB<enemyColor, Bishop, Queen>();

    // Giá trị checkmask = 0xff.ff. Nếu đang chiếu sẽ bằng giá trị từ quân cờ tấn công đến vua (trừ vua)
    const BB checkMask = checkers
            ? attack::between[myKingIdx][lsbIndex(checkers)]
            : All_BB;


    // Tính toán toàn bộ các vị trí pin
    const BB pinMaskDiagonal = getPinMaskDiagonal();
    const BB pinMaskCross = getPinMaskCross();
//    BB enemyBishopQueenHasXRayOnKing = attack::getBishopXRay(myKingIdx, occ) & _enemyBishopQueens;
//    while (enemyBishopQueenHasXRayOnKing) {
//        pinMaskDiagonal |= attack::between[myKingIdx][popLsb(enemyBishopQueenHasXRayOnKing)];
//    }
//    BB enemyRookQueenHasXRayOnKing = attack::getRookXRay(myKingIdx, occ) & _enemyRookQueens;
//    while (enemyRookQueenHasXRayOnKing) {
//        pinMaskCross |= attack::between[myKingIdx][popLsb(enemyRookQueenHasXRayOnKing)];
//    }

    int count = 0;
    const BB myPawns = board.getPieceBB<mine, Pawn>();

    const BB kingRank = rankBB(getRank(Square(myKingIdx)));
    constexpr Direction up = mine == White ? North : South;
    constexpr Direction up2 = mine == White ? Direction(North * 2) : Direction(South * 2);
    constexpr Direction uLeft = mine == White ? NorthWest : SouthWest;
    constexpr Direction uRight = mine == White ? NorthEast : SouthEast;
    constexpr BB rank3 = rankBB(mine == White ? Rank3 : Rank6);

    const BB pinForPawnPush = pinMaskCross & kingRank;
    const BB pinLeft = getSideDiag<enemyColor>(myKingIdx) & pinMaskDiagonal;
    const BB pinRight = getSideDiag<mine>(myKingIdx) & pinMaskDiagonal;

    // Tìm danh sách các tốt ở dòng 7
    constexpr auto rank7 = mine == White ? rankBB(Rank7) : rankBB(Rank2);
    const BB myPawnOn7 = myPawns & rank7;
    const BB myPawnNotOn7 = myPawns & ~rank7;

    BB myPushablePawnOn7 = myPawnOn7 & ~pinMaskDiagonal;
    BB myAttackablePawnOn7 = myPawnOn7 & ~pinMaskCross;
    BB myPushablePawnNotOn7 = myPawnNotOn7 & ~pinMaskDiagonal;
    BB myAttackablePawnNotOn7 = myPawnNotOn7 & ~pinMaskCross;

    const BB pawnPushTarget = notOcc & checkMask;
    const BB pawnAttackTarget = enemies & checkMask;
    {
        BB p1 = shift<up>(myPushablePawnOn7 & ~pinForPawnPush) & pawnPushTarget;
        while (p1){
            const int to = popLsb(p1);
            const int from = to - up;
            moveList[count++] = Move::makePromotionMove(from, to, Queen);
            moveList[count++] = Move::makePromotionMove(from, to, Rook);
            moveList[count++] = Move::makePromotionMove(from, to, Bishop);
            moveList[count++] = Move::makePromotionMove(from, to, Knight);
        }
    }

    {
        BB pl = shift<uLeft>(myAttackablePawnOn7 & ~pinLeft) & pawnAttackTarget;
        BB pr = shift<uRight>(myAttackablePawnOn7 & ~pinRight) & pawnAttackTarget;

        while (pl){
            const int to = popLsb(pl);
            const int from = to - uLeft;
            moveList[count++] = Move::makePromotionMove(from, to, Queen);
            moveList[count++] = Move::makePromotionMove(from, to, Rook);
            moveList[count++] = Move::makePromotionMove(from, to, Bishop);
            moveList[count++] = Move::makePromotionMove(from, to, Knight);
        }
        while (pr){
            const int to = popLsb(pr);
            const int from = to - uRight;
            moveList[count++] = Move::makePromotionMove(from, to, Queen);
            moveList[count++] = Move::makePromotionMove(from, to, Rook);
            moveList[count++] = Move::makePromotionMove(from, to, Bishop);
            moveList[count++] = Move::makePromotionMove(from, to, Knight);
        }
    }


    {
        BB p1 = shift<up>(myPushablePawnNotOn7 & ~pinForPawnPush) & notOcc;
        BB p2 = shift<up>(p1 & rank3) & notOcc & checkMask;
        p1 &= checkMask;
        while (p1){
            const int to = popLsb(p1);
            moveList[count++] = Move::makeNormalMove(to - up, to);
        }
        while (p2){
            const int to = popLsb(p2);
            moveList[count++] = Move::makeNormalMove(to - up2, to);
        }
    }

    {
        BB pl = shift<uLeft>(myAttackablePawnNotOn7 & ~pinLeft) & pawnAttackTarget;
        BB pr = shift<uRight>(myAttackablePawnNotOn7 & ~pinRight) & pawnAttackTarget;

        while (pl){
            const int to = popLsb(pl);
            moveList[count++] = Move::makeNormalMove(to - uLeft, to);
        }
        while (pr){
            const int to = popLsb(pr);
            moveList[count++] = Move::makeNormalMove(to - uRight, to);
        }
        constexpr BB enPassantRankBB = mine == White ? rankBB(Rank5) : rankBB(Rank4);

        const BB enPassantBB = indexToBB(board.state->enPassant);
        if (board.state->enPassant != SquareNone
                && (pinMaskDiagonal & enPassantBB) == 0){
            BB pawnCanAttackEP =  myAttackablePawnNotOn7
                    & enPassantRankBB
                    & ~pinMaskDiagonal
                    & ((enPassantBB << 1) | (enPassantBB >> 1));
            while (pawnCanAttackEP){
                const int index = popLsb(pawnCanAttackEP);
                const BB from = indexToBB(index);
                const BB occWithout2Pawn = occ & ~(enPassantBB | from);
                BB rookQueenInRanks = attack::getRookAttacks(myKingIdx, occWithout2Pawn) & _enemyRookQueens & kingRank;
                if (!rookQueenInRanks){
                    moveList[count++] = Move::makeEnpassantMove(index, board.state->enPassantTarget<mine>());
                }
            }
        }
    }

    const BB target = notMines & checkMask;
    BB myKnights = board.getPieceBB<mine, Knight>() & ~(pinMaskCross | pinMaskDiagonal);
    while (myKnights){
        const int i = popLsb(myKnights);
        BB attack = attack::knights[i] & target;
        while (attack){
            moveList[count++] = Move::makeNormalMove(i, popLsb(attack));
        }
    }

    BB myBishopQueens = board.getPieceBB<mine, Bishop, Queen>() & ~pinMaskCross;
    while (myBishopQueens){
        const int i = popLsb(myBishopQueens);
        const BB from = indexToBB(i);
        BB attack = attack::getBishopAttacks(i, occ) & target
                & (~setAllBit(pinMaskDiagonal & from) | pinMaskDiagonal);
        while (attack){
            moveList[count++] = Move::makeNormalMove(i, popLsb(attack));
        }
    }

    BB myRookQueens = board.getPieceBB<mine, Rook, Queen>() & ~pinMaskDiagonal;
    while (myRookQueens){
        int i = popLsb(myRookQueens);
        BB from = indexToBB(i);
        BB attack = attack::getRookAttacks(i, occ) & target
                & (~setAllBit(pinMaskCross & from) | pinMaskCross);
        while (attack){
            moveList[count++] = Move::makeNormalMove(i, popLsb(attack));
        }
    }

    // Tính toán toàn bộ các nước đi mà vua bị không được phép di chuyển tới
    BB kingBan = getKingBan<mine>(board);
    BB kingAttacks = attack::kings[myKingIdx] & notMines & ~kingBan;
    while (kingAttacks){
        moveList[count++] = Move::makeNormalMove(myKingIdx, popLsb(kingAttacks));
    }
    if constexpr (mine == White) {
        if ((occ & castlingSpace<CastlingWK>()) == 0
                && (kingBan & castlingKingPath<CastlingWK>()) == 0
                && (board.state->castlingRights & CastlingWK)){
            constexpr static auto castlingSrc = getCastlingIndex<CastlingWK, King, true>();
            constexpr static auto castlingDst = getCastlingIndex<CastlingWK, King, false>();
            constexpr static auto castlingMove = Move::makeCastlingMove(castlingSrc, castlingDst);
            moveList[count++] = castlingMove;
        }
        if ((occ & castlingSpace<CastlingWQ>()) == 0
                && (kingBan & castlingKingPath<CastlingWQ>()) == 0
                && (board.state->castlingRights & CastlingWQ)){
            constexpr static auto castlingSrc = getCastlingIndex<CastlingWQ, King, true>();
            constexpr static auto castlingDst = getCastlingIndex<CastlingWQ, King, false>();
            constexpr static auto castlingMove = Move::makeCastlingMove(castlingSrc, castlingDst);
            moveList[count++] = castlingMove;
        }
    } else {
        if ((occ & castlingSpace<CastlingBK>()) == 0
                && (kingBan & castlingKingPath<CastlingBK>()) == 0
                && (board.state->castlingRights & CastlingBK)){
            constexpr static auto castlingSrc = getCastlingIndex<CastlingBK, King, true>();
            constexpr static auto castlingDst = getCastlingIndex<CastlingBK, King, false>();
            constexpr static auto castlingMove = Move::makeCastlingMove(castlingSrc, castlingDst);
            moveList[count++] = castlingMove;
        }
        if ((occ & castlingSpace<CastlingBQ>()) == 0
                && (kingBan & castlingKingPath<CastlingBQ>()) == 0
                && (board.state->castlingRights & CastlingBQ)){
            constexpr static auto castlingSrc = getCastlingIndex<CastlingBQ, King, true>();
            constexpr static auto castlingDst = getCastlingIndex<CastlingBQ, King, false>();
            constexpr static auto castlingMove = Move::makeCastlingMove(castlingSrc, castlingDst);
            moveList[count++] = castlingMove;
        }
    }

    return count;

}


template<Color mine>
BB MoveGen::getKingBan(const Board &board) noexcept
{
    constexpr auto enemyColor = !mine;
    const BB occWithoutOurKing = occ & (~myKing);

    const BB enemyPawns = board.getPieceBB<enemyColor, Pawn>();
    const BB enemyKnights = board.getPieceBB<enemyColor, Knight>();
    const BB enemyKings = board.getPieceBB<enemyColor, King>();
    BB enemyBishopQueens = board.getPieceBB<enemyColor, Bishop, Queen>();
    BB enemyRookQueens = board.getPieceBB<enemyColor, Rook, Queen>();

    // Tính toán toàn bộ các nước đi mà vua bị không được phép di chuyển tới
    BB kingBan = attack::getPawnAttacks<enemyColor>(enemyPawns)
            | attack::getKnightAttacks(enemyKnights)
            | attack::getKingAttacks(enemyKings);

    while (enemyBishopQueens) {
        kingBan |= attack::getBishopAttacks(popLsb(enemyBishopQueens), occWithoutOurKing);
    }
    while (enemyRookQueens) {
        kingBan |= attack::getRookAttacks(popLsb(enemyRookQueens), occWithoutOurKing);
    }
    return kingBan;
}


template<Color mine>
int MoveGen::getMoveListWhenDoubleCheck(const Board &board, Move *moveList) noexcept {
    auto kingBan = getKingBan<mine>(board);

    // Trường hợp double check, chỉ cho phép di chuyển vua
    BB kingAttacks = attack::kings[myKingIdx] & notMines & (~kingBan);
    int count = 0;
    while (kingAttacks){
        moveList[count++] = Move::makeNormalMove(myKingIdx, popLsb(kingAttacks));
    }
    return count;
}



std::vector<Move> MoveGen::getMoveListForSquare(const Board &board, Square square){
    Move moves[256];
    int count = genMoveList(board, moves);
    qDebug() << "Gen move list " << count;
    std::vector<Move> output;
    for (int i = 0; i < count; i++){
        const auto &move = moves[i];
        if (move.src() == square){
            output.push_back(moves[i]);
        }
    }
    return output;
}


