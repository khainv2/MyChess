#include "movegen.h"
#include "attack.h"
#include <bitset>
#include <QDebug>
#include <QElapsedTimer>
#include "evaluation.h"
#include "util.h"

using namespace kc;


//template
int kc::genMoveList(const Board &board, Move *moveList) noexcept
{
    if (board.side == White){
        return genMoveList<White>(board, moveList);
    } else {
        return genMoveList<Black>(board, moveList);
    }
}


template<Color color>
int kc::getMoveListWhenDoubleCheck(const Board &board, Move *moveList) noexcept
{
    constexpr auto enemyColor = !color;
    const BB occ = board.getOccupancy();

    // Lấy vị trí của vua hiện tại
    const BB myKing = board.getPieceBB<color, King>();
    const int myKingIdx = lsbIndex(myKing);
    const BB occWithoutOurKing = occ & (~myKing); // Trong trường hợp slide attack vẫn có thể 'nhìn' các vị trí sau vua

    const BB mines = board.getMines<color>();
    const BB notMines = ~mines;
    const BB _enemyRookQueens = board.getPieceBB<enemyColor, Rook, Queen>();
    const BB _enemyBishopQueens = board.getPieceBB<enemyColor, Bishop, Queen>();

    // Tính toán toàn bộ các nước đi mà vua bị không được phép di chuyển tới
    BB kingBan = attack::getPawnAttacks<enemyColor>(board.getPieceBB<enemyColor, Pawn>())
            | attack::getKnightAttacks(board.getPieceBB<enemyColor, Knight>())
            | attack::getKingAttacks(board.getPieceBB<enemyColor, King>());

    BB enemyBishopQueens = _enemyBishopQueens;
    BB enemyRookQueens = _enemyRookQueens;
    while (enemyBishopQueens) {
        kingBan |= attack::getBishopAttacks(popLsb(enemyBishopQueens), occWithoutOurKing);
    }
    while (enemyRookQueens) {
        kingBan |= attack::getRookAttacks(popLsb(enemyRookQueens), occWithoutOurKing);
    }

    // Trường hợp double check, chỉ cho phép di chuyển vua
    BB kingAttacks = attack::kings[myKingIdx] & notMines & (~kingBan);
    int count = 0;
    while (kingAttacks){
        moveList[count++] = Move::makeNormalMove(myKingIdx, popLsb(kingAttacks));
    }
    return count;
}


template<Color color>
int kc::genMoveList(const Board &board, Move *moveList) noexcept
{
    constexpr auto enemyColor = !color;
    const BB occ = board.getOccupancy();
    const BB notOcc = ~occ;

    // Lấy vị trí của vua hiện tại
    const BB myKing = board.getPieceBB<color, King>();
    const int myKingIdx = lsbIndex(myKing);
    const BB occWithoutOurKing = occ & (~myKing); // Trong trường hợp slide attack vẫn có thể 'nhìn' các vị trí sau vua

    const BB mines = board.getMines<color>();
    const BB notMines = ~mines;
    const BB enemies = board.getEnemies<color>();
    const BB sqAttackToMyKing = board.getSqAttackTo(myKingIdx, occ) & enemies;
    const BB _enemyRookQueens = board.getPieceBB<enemyColor, Rook, Queen>();
    const BB _enemyBishopQueens = board.getPieceBB<enemyColor, Bishop, Queen>();
    if (isMoreThanOne(sqAttackToMyKing)) {
        // Tính toán toàn bộ các nước đi mà vua bị không được phép di chuyển tới
        BB kingBan = attack::getPawnAttacks<enemyColor>(board.getPieceBB<enemyColor, Pawn>())
                | attack::getKnightAttacks(board.getPieceBB<enemyColor, Knight>())
                | attack::getKingAttacks(board.getPieceBB<enemyColor, King>());

        BB enemyBishopQueens = _enemyBishopQueens;
        BB enemyRookQueens = _enemyRookQueens;
        while (enemyBishopQueens) {
            kingBan |= attack::getBishopAttacks(popLsb(enemyBishopQueens), occWithoutOurKing);
        }
        while (enemyRookQueens) {
            kingBan |= attack::getRookAttacks(popLsb(enemyRookQueens), occWithoutOurKing);
        }

        // Trường hợp double check, chỉ cho phép di chuyển vua
        BB kingAttacks = attack::kings[myKingIdx] & notMines & (~kingBan);
        int count = 0;
        while (kingAttacks){
            moveList[count++] = Move::makeNormalMove(myKingIdx, popLsb(kingAttacks));
        }
        return count;
    } else {

        // Giá trị checkmask = 0xff.ff. Nếu đang chiếu sẽ bằng giá trị từ quân cờ tấn công đến vua (trừ vua)
        BB checkMask;// = ~setAllBit(sqAttackToMyKing) | attack::between[myKingIdx][lsbIndex(sqAttackToMyKing)];
        if (sqAttackToMyKing){
            checkMask = attack::between[myKingIdx][lsbIndex(sqAttackToMyKing)];
        } else {
            checkMask = All_BB;
        }

        // Tính toán toàn bộ các vị trí pin
        BB pinMaskDiagonal = 0;
        BB pinMaskCross = 0;
        BB enemyBishopQueens = _enemyBishopQueens;
        while (enemyBishopQueens) {
            const int index = popLsb(enemyBishopQueens);
            const bool isKingPinned = attack::getBishopXRay(index, occ) & myKing;
            pinMaskDiagonal |= (attack::between[myKingIdx][index] & setAllBit(isKingPinned));
        }
        BB enemyRookQueens = _enemyRookQueens;
        while (enemyRookQueens){
            const int index = popLsb(enemyRookQueens);
            const bool isKingPinned = attack::getRookXRay(index, occ) & myKing;
            pinMaskCross |= (attack::between[myKingIdx][index] & setAllBit(isKingPinned));
        }

        int count = 0;
        BB myPawns = board.getPieceBB<color, Pawn>();

        const BB kingRank = rankBB(getRank(Square(myKingIdx)));
        constexpr Direction up = color == White ? North : South;
        constexpr Direction up2 = color == White ? Direction(North * 2) : Direction(South * 2);
        constexpr Direction uLeft = color == White ? NorthWest : SouthWest;
        constexpr Direction uRight = color == White ? NorthEast : SouthEast;
        constexpr BB rank3 = rankBB(color == White ? Rank3 : Rank6);


        // Tìm danh sách các tốt ở dòng 7
        constexpr auto rank7 = color == White ? rankBB(Rank7) : rankBB(Rank2);
        BB myPawnOn7 = myPawns & rank7;
        BB myPawnNotOn7 = myPawns & ~rank7;

        BB myPushablePawnOn7 = myPawnOn7 & ~pinMaskDiagonal;

        {
            const BB kingRank = rankBB(getRank(Square(myKingIdx)));
            BB pinForPawnPush = pinMaskCross & kingRank;
            BB p1 = shift<up>(myPushablePawnOn7 & ~pinForPawnPush) & notOcc & checkMask;
            while (p1){
                int to = popLsb(p1);
                int from = to - up;
                moveList[count++] = Move::makePromotionMove(from, to, Queen);
                moveList[count++] = Move::makePromotionMove(from, to, Rook);
                moveList[count++] = Move::makePromotionMove(from, to, Bishop);
                moveList[count++] = Move::makePromotionMove(from, to, Knight);
            }
        }

        BB myAttackablePawnOn7 = myPawnOn7 & ~pinMaskCross;
        {
            const BB pinLeft = getSideDiag<enemyColor>(myKingIdx) & pinMaskDiagonal;
            const BB pinRight = getSideDiag<color>(myKingIdx) & pinMaskDiagonal;
            BB pl = shift<uLeft>(myAttackablePawnOn7 & ~pinLeft) & enemies & checkMask;
            BB pr = shift<uRight>(myAttackablePawnOn7 & ~pinRight) & enemies & checkMask;

            while (pl){
                int to = popLsb(pl);
                moveList[count++] = Move::makePromotionMove(to - uLeft, to, Queen);
                moveList[count++] = Move::makePromotionMove(to - uLeft, to, Rook);
                moveList[count++] = Move::makePromotionMove(to - uLeft, to, Bishop);
                moveList[count++] = Move::makePromotionMove(to - uLeft, to, Knight);
            }
            while (pr){
                int to = popLsb(pr);
                moveList[count++] = Move::makePromotionMove(to - uRight, to, Queen);
                moveList[count++] = Move::makePromotionMove(to - uRight, to, Rook);
                moveList[count++] = Move::makePromotionMove(to - uRight, to, Bishop);
                moveList[count++] = Move::makePromotionMove(to - uRight, to, Knight);
            }
        }

        BB myPushablePawnNotOn7 = myPawnNotOn7 & ~pinMaskDiagonal;
        BB myAttackablePawnNotOn7 = myPawnNotOn7 & ~pinMaskCross;

        {
            BB pinForPawnPush = pinMaskCross & kingRank;

            BB p1 = shift<up>(myPushablePawnNotOn7 & ~pinForPawnPush) & notOcc;
            BB p2 = shift<up>(p1 & rank3) & notOcc;
            p1 &= checkMask;
            p2 &= checkMask;
            while (p1){
                int to = popLsb(p1);
                moveList[count++] = Move::makeNormalMove(to - up, to);
            }
            while (p2){
                int to = popLsb(p2);
                moveList[count++] = Move::makeNormalMove(to - up2, to);
            }
        }

        {
            const BB pinLeft = getSideDiag<enemyColor>(myKingIdx) & pinMaskDiagonal;
            const BB pinRight = getSideDiag<color>(myKingIdx) & pinMaskDiagonal;
            BB pl = shift<uLeft>(myAttackablePawnNotOn7 & ~pinLeft) & enemies & checkMask;
            BB pr = shift<uRight>(myAttackablePawnNotOn7 & ~pinRight) & enemies & checkMask;

            while (pl){
                int to = popLsb(pl);
                moveList[count++] = Move::makeNormalMove(to - uLeft, to);
            }
            while (pr){
                int to = popLsb(pr);
                moveList[count++] = Move::makeNormalMove(to - uRight, to);
            }

            if (board.state->enPassant != SquareNone
                    && (pinMaskDiagonal & indexToBB(board.state->enPassant)) == 0){
                BB enPassantBB = indexToBB(board.state->enPassant);
                BB pawnCanAttackEP =  myAttackablePawnNotOn7
                        & rankBB(getRank(board.state->enPassant))
                        & ((enPassantBB << 1) | (enPassantBB >> 1));
                while (pawnCanAttackEP){
                    int index = popLsb(pawnCanAttackEP);
                    BB from = indexToBB(index);
                    // Kiểm tra thêm trường hợp ngang hàng tốt có hậu hoặc xe đang tấn công xuyên 2 tốt
                    Rank ourKingRank = getRank(Square(myKingIdx));
                    BB ourKingRankBB = rankBB(ourKingRank);
                    BB enemieRookOrQueenInRanks = _enemyRookQueens & ourKingRankBB ;
                    BB occWithout2Pawn = occ & (~indexToBB(board.state->enPassant)) & (~from);
                    bool isKingSeenByEnemyRookOrQueen = false;
                    while (enemieRookOrQueenInRanks){
                        int sqRQ = popLsb(enemieRookOrQueenInRanks);
                        BB rookQueenAttack = attack::getRookAttacks(sqRQ, occWithout2Pawn);
                        if (rookQueenAttack & myKing){
                            isKingSeenByEnemyRookOrQueen = true;
                            break;
                        }
                    }

                    if (!isKingSeenByEnemyRookOrQueen){
                        moveList[count++] = Move::makeEnpassantMove(index, board.state->enPassantTarget<color>());
                    }
                }
            }

        }

        const BB target = notMines & checkMask;
        BB myKnights = board.getPieceBB<color, Knight>() & ~(pinMaskCross | pinMaskDiagonal);
        while (myKnights){
            int i = popLsb(myKnights);
            BB attack = attack::knights[i] & target;
            while (attack){
                moveList[count++] = Move::makeNormalMove(i, popLsb(attack));
            }
        }

        BB myBishopQueens = board.getPieceBB<color, Bishop, Queen>() & ~pinMaskCross;
        while (myBishopQueens){
            int i = popLsb(myBishopQueens);
            BB from = indexToBB(i);
            BB attack = attack::getBishopAttacks(i, occ) & target;
            if (pinMaskDiagonal & from){
                attack &= pinMaskDiagonal;
            }
            while (attack){
                moveList[count++] = Move::makeNormalMove(i, popLsb(attack));
            }
        }

        BB myRookQueens = board.getPieceBB<color, Rook, Queen>() & ~pinMaskDiagonal;
        while (myRookQueens){
            int i = popLsb(myRookQueens);
            BB from = indexToBB(i);
            BB attack = attack::getRookAttacks(i, occ) & target;
            if (pinMaskCross & from){
                attack &= pinMaskCross;
            }
            while (attack){
                moveList[count++] = Move::makeNormalMove(i, popLsb(attack));
            }
        }

        BB kingAttacks = attack::kings[myKingIdx] & notMines;
        if (kingAttacks){
            // Tính toán toàn bộ các nước đi mà vua bị không được phép di chuyển tới
            BB kingBan = attack::getPawnAttacks<enemyColor>(board.getPieceBB<enemyColor, Pawn>())
                    | attack::getKnightAttacks(board.getPieceBB<enemyColor, Knight>())
                    | attack::getKingAttacks(board.getPieceBB<enemyColor, King>());

            BB enemyBishops = _enemyBishopQueens;
            while (enemyBishops) {
                kingBan |= attack::getBishopAttacks(popLsb(enemyBishops), occWithoutOurKing);
            }
            BB enemyRooks = _enemyRookQueens;
            while (enemyRooks) {
                kingBan |= attack::getRookAttacks(popLsb(enemyRooks), occWithoutOurKing);
            }

            kingAttacks &= ~kingBan;
            while (kingAttacks){
                moveList[count++] = Move::makeNormalMove(myKingIdx, popLsb(kingAttacks));
            }
            if constexpr (color == White) {
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
        }

        return count;
    }
}

std::vector<Move> kc::getMoveListForSquare(const Board &board, Square square){
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





