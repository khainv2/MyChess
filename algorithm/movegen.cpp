#include "movegen.h"
#include "attack.h"
#include <bitset>
#include <QDebug>
#include <QElapsedTimer>
#include "evaluation.h"
#include "util.h"

#define TEST_PERFT
using namespace kc;

//#define COUNT_TIME_PERFT

//template
int kc::generateMoveList(const Board &board, Move *moveList)
{
    if (board.side == White){
        return genMoveList<White>(board, moveList);
    } else {
        return genMoveList<Black>(board, moveList);
    }
}

template<Color color>
int kc::genMoveList(const Board &board, Move *moveList)
{
#ifdef COUNT_TIME_PERFT
    std::vector<u64> times;
    times.resize(20);
    auto ptr = times.data();
    int c = 0;

    QElapsedTimer timer;
    timer.start();
#endif
    constexpr auto enemyColor = !color;
    const BB occ = board.getOccupancy();
    const BB notOcc = ~occ;

    // Lấy vị trí của vua hiện tại
    const BB myKing = board.getPieceBB<color, King>();
    const int myKingIdx = lsbIndex(myKing);
    const BB occWithoutOurKing = occ & (~myKing); // Trong trường hợp slide attack vẫn có thể 'nhìn' các vị trí sau vua

#ifdef COUNT_TIME_PERFT
    ptr[c++] = timer.nsecsElapsed();
#endif

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

#ifdef COUNT_TIME_PERFT
        ptr[c++] = timer.nsecsElapsed();
#endif
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

#ifdef COUNT_TIME_PERFT
        ptr[c++] = timer.nsecsElapsed();
#endif


        const BB notOccShift8ByColor = color == White ? (notOcc << 8) : (notOcc >> 8);
        int count = 0;
        const BB pawnPush2Mask = notOcc & notOccShift8ByColor;

        BB myPawns = board.getPieceBB<color, Pawn>();


        const BB kingRank = rankBB(getRank(Square(myKingIdx)));
        constexpr Direction up = color == White ? North : South;
        constexpr Direction up2 = color == White ? Direction(North * 2) : Direction(South * 2);
        constexpr Direction uLeft = color == White ? NorthWest : SouthWest;
        constexpr Direction uRight = color == White ? NorthEast : SouthEast;
        constexpr BB rank2 = rankBB(color == White ? Rank2 : Rank7);
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
        while (myAttackablePawnOn7){
            int index = popLsb(myAttackablePawnOn7);
            BB from = indexToBB(index);
            BB attack = attack::getPawnAttacks<color>(from) & enemies & checkMask;
            if (from & pinMaskDiagonal){
                attack &= pinMaskDiagonal;
            }
            while (attack){
                int to = popLsb(attack);
                moveList[count++] = Move::makePromotionMove(index, to, Queen);
                moveList[count++] = Move::makePromotionMove(index, to, Rook);
                moveList[count++] = Move::makePromotionMove(index, to, Bishop);
                moveList[count++] = Move::makePromotionMove(index, to, Knight);
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
//            BB pl =
        }

        // Tính toán nước đi cho tất cả các tốt không bị ghim theo chiều ngang
        while (myAttackablePawnNotOn7){
            int index = popLsb(myAttackablePawnNotOn7);
            BB from = indexToBB(index);
            BB attack = attack::getPawnAttacks<color>(from) & enemies & checkMask;
            if (from & pinMaskDiagonal){
                attack &= pinMaskDiagonal;
            }
            while (attack){
                moveList[count++] = Move::makeNormalMove(index, popLsb(attack));
            }
            bool enPassantCond = board.state->enPassant != SquareNone // Có trạng thái một tốt vừa được push 2
                    && ((board.state->enPassant / 8) == (index / 8)) // Ô tốt push 2 ở cùng hàng với ô from
                    && ((board.state->enPassant - index) == 1 || (index - board.state->enPassant == 1)) // 2 ô cách nhau 1 đơn vị
                    && ((pinMaskDiagonal & indexToBB(board.state->enPassant)) == 0);
            if (enPassantCond){
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



//        // Tính toán nước đi cho tất cả các tốt không bị ghim theo đường chéo
//        BB myPushablePawns = myPawns & ~pinMaskDiagonal;
////        qDebug() << bbToString(myPushablePawns).c_str();
//        while (myPushablePawns){
//            int index = popLsb(myPushablePawns);
//            BB from = indexToBB(index);
//            BB attack = ((attack::getPawnPush<color>(from) & notOcc)
//                         | (attack::getPawnPush2<color>(from) & pawnPush2Mask)) & checkMask;
//            if (pinMaskCross & from){
//                attack &= pinMaskCross;
//            }
////            qDebug() << "attack"<< bbToString(pinMaskCross).c_str();
//            constexpr auto rankBeforePromote = color == White ? rankBB(Rank7) : rankBB(Rank2);
//            const bool promotionCond = from & rankBeforePromote;
//            if (promotionCond){
//                while (attack){
//                    int to = popLsb(attack);
//                    moveList[count++] = Move::makePromotionMove(index, to, Queen);
//                    moveList[count++] = Move::makePromotionMove(index, to, Rook);
//                    moveList[count++] = Move::makePromotionMove(index, to, Bishop);
//                    moveList[count++] = Move::makePromotionMove(index, to, Knight);
//                }
//            } else {
//                while (attack){
//                    moveList[count++] = Move::makeNormalMove(index, popLsb(attack));
//                }
//            }
//        }

//        // Tính toán nước đi cho tất cả các tốt không bị ghim theo chiều ngang
//        BB myAttackablePawns = myPawns & ~pinMaskCross;
//        while (myAttackablePawns){
//            int index = popLsb(myAttackablePawns);
//            BB from = indexToBB(index);
//            BB attack = attack::getPawnAttacks<color>(from) & enemies & checkMask;
//            if (from & pinMaskDiagonal){
//                attack &= pinMaskDiagonal;
//            }
//            constexpr auto rankBeforePromote = color == White ? rankBB(Rank7) : rankBB(Rank2);
//            const bool promotionCond = from & rankBeforePromote;
//            if (promotionCond){
//                while (attack){
//                    int to = popLsb(attack);
//                    moveList[count++] = Move::makePromotionMove(index, to, Queen);
//                    moveList[count++] = Move::makePromotionMove(index, to, Rook);
//                    moveList[count++] = Move::makePromotionMove(index, to, Bishop);
//                    moveList[count++] = Move::makePromotionMove(index, to, Knight);
//                }
//            } else {
//                while (attack){
//                    moveList[count++] = Move::makeNormalMove(index, popLsb(attack));
//                }
//            }

//            bool enPassantCond = board.state->enPassant != SquareNone // Có trạng thái một tốt vừa được push 2
//                    && ((board.state->enPassant / 8) == (index / 8)) // Ô tốt push 2 ở cùng hàng với ô from
//                    && ((board.state->enPassant - index) == 1 || (index - board.state->enPassant == 1)) // 2 ô cách nhau 1 đơn vị
//                    && ((pinMaskDiagonal & indexToBB(board.state->enPassant)) == 0)

//                    ;
//            if (enPassantCond){
//                // Kiểm tra thêm trường hợp ngang hàng tốt có hậu hoặc xe đang tấn công xuyên 2 tốt
//                Rank ourKingRank = getRank(Square(myKingIdx));
//                BB ourKingRankBB = rankBB(ourKingRank);

//                BB enemieRookOrQueenInRanks = _enemyRookQueens & ourKingRankBB ;

//                BB occWithout2Pawn = occ & (~indexToBB(board.state->enPassant)) & (~from);
//                bool isKingSeenByEnemyRookOrQueen = false;
//                while (enemieRookOrQueenInRanks){
//                    int sqRQ = popLsb(enemieRookOrQueenInRanks);
//                    BB rookQueenAttack = attack::getRookAttacks(sqRQ, occWithout2Pawn);
//                    if (rookQueenAttack & myKing){
//                        isKingSeenByEnemyRookOrQueen = true;
//                        break;
//                    }
//                }

//                if (!isKingSeenByEnemyRookOrQueen){
//                    moveList[count++] = Move::makeEnpassantMove(index, board.state->enPassantTarget<color>());
//                }
//            }
//        }


//        while (myPawns){
//            const u64 from = lsbBB(myPawns);
//            const int index = popLsb(myPawns);

//            u64 attack = 0;
//            if (pinMaskDiagonal & from){

//                attack = attack::getPawnAttacks<color>(from) & enemies & checkMask & pinMaskDiagonal;
//            }
//            else if (pinMaskCross & from){
//                attack = ((attack::getPawnPush<color>(from) & notOcc)
//                        | (attack::getPawnPush2<color>(from) & pawnPush2Mask)) & checkMask & pinMaskCross;
//            }
//            else {
//                attack = (attack::pawns[color][index] & enemies)
//                       | (attack::pawnPushes[color][index] & notOcc)
//                       | (attack::pawnPushes2[color][index] & pawnPush2Mask);
//                attack &= checkMask;
//            }
//            bool enPassantCond = board.state->enPassant != SquareNone // Có trạng thái một tốt vừa được push 2
//                    && ((board.state->enPassant / 8) == (index / 8)) // Ô tốt push 2 ở cùng hàng với ô from
//                    && ((board.state->enPassant - index) == 1 || (index - board.state->enPassant == 1)) // 2 ô cách nhau 1 đơn vị
//                    && ((pinMaskDiagonal & indexToBB(board.state->enPassant)) == 0) // Ô tấn công không bị pin theo đường chéow
//                    && ((pinMaskCross & from) == 0)
//                    && ((pinMaskDiagonal & from) == 0)
//                    ;
//            if (enPassantCond){
//                // Kiểm tra thêm trường hợp ngang hàng tốt có hậu hoặc xe đang tấn công xuyên 2 tốt
//                Rank ourKingRank = getRank(Square(myKingIdx));
//                BB ourKingRankBB = rankBB(ourKingRank);

//                BB enemieRookOrQueenInRanks = _enemyRookQueens & ourKingRankBB ;

//                BB occWithout2Pawn = occ & (~indexToBB(board.state->enPassant)) & (~from);
//                bool isKingSeenByEnemyRookOrQueen = false;
//                while (enemieRookOrQueenInRanks){
//                    int sqRQ = popLsb(enemieRookOrQueenInRanks);
//                    BB rookQueenAttack = attack::getRookAttacks(sqRQ, occWithout2Pawn);
//                    if (rookQueenAttack & myKing){
//                        isKingSeenByEnemyRookOrQueen = true;
//                        break;
//                    }
//                }

//                if (!isKingSeenByEnemyRookOrQueen){
//                    moveList[count++] = Move::makeEnpassantMove(index, board.state->enPassantTarget<color>());
//                }
//            }
//            constexpr auto rankBeforePromote = color == White ? rankBB(Rank7) : rankBB(Rank2);
//            const bool promotionCond = from & rankBeforePromote;
//            if (promotionCond){
//                while (attack){
//                    int to = popLsb(attack);
//                    moveList[count++] = Move::makePromotionMove(index, to, Queen);
//                    moveList[count++] = Move::makePromotionMove(index, to, Rook);
//                    moveList[count++] = Move::makePromotionMove(index, to, Bishop);
//                    moveList[count++] = Move::makePromotionMove(index, to, Knight);
//                }
//            } else {
//                while (attack){
//                    moveList[count++] = Move::makeNormalMove(index, popLsb(attack));
//                }
//            }
//        }

#ifdef COUNT_TIME_PERFT
        ptr[c++] = timer.nsecsElapsed();
#endif

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

#ifdef COUNT_TIME_PERFT
        ptr[c++] = timer.nsecsElapsed();
#endif


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


#ifdef COUNT_TIME_PERFT
        ptr[c++] = timer.nsecsElapsed();
        for (int i = 0; i < c; i++){
            const auto &p = ptr[i];
            qDebug() << "Calc" << p;
        }
#endif

        return count;

    }

}


std::vector<Move> kc::getMoveListForSquare(const Board &board, Square square){
    Move moves[256];
    int count = generateMoveList(board, moves);
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

constexpr static int FixedDepth = 10;
int countMate = 0;
int countCapture = 0;
int countCheck = 0;
std::map<std::string, int> divideCount;
std::string currMove;
QElapsedTimer myTimer;
qint64 genTick = 0;
qint64 moveTick = 0;
int countMoveExt = 0;

std::string kc::testFenPerft()
{
//    return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
//    return "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 2 1"; // Root

    return "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P1P1/2N2Q1p/PPPBBP1P/R3K2R b KQkq g4 0 10";
}
#define MULTI_LINE_STRING(a) #a

void kc::testPerft()
{
//    return;
    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
    // rnbqk1nr/pppp1ppp/8/P3p3/1b6/8/1PPPPPPP/RNBQKBNR w KQkq - 1 3
//    parseFENString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &board);
    int totalCount = 0;
    auto funcTest = [&](const std::string fen, int depth, int expectedTotalMove){
        QElapsedTimer timer;
        kc::Board board;
        parseFENString(fen, &board);
        timer.start();
        int moveCount = genMoveRecur(board, depth);
        if (moveCount != expectedTotalMove){
            qWarning() << "Wrong val when test perft";
            qWarning() << fen.c_str() << depth << moveCount << "expected" << expectedTotalMove;
        } else {
            qDebug() << "Test perft done" << fen.c_str() << "on" << (timer.nsecsElapsed() / 1000000) << "ms";
        }
        totalCount += moveCount;
    };
    bool test = false;
#ifdef TEST_PERFT
    test = true;
#endif


//    funcTest("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039);

    if (test){
        QElapsedTimer timer;
        timer.start();
        funcTest("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609);
//        funcTest("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, 119060324);
        funcTest("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603);
        funcTest("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594);
        funcTest("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 10", 5, 674624);
        funcTest("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487);

        qint64 t = timer.nsecsElapsed() / 1000000;
        qDebug() << "Time for multi board" << t << "ms";
        qDebug() << "Count node per sec=" << ((totalCount / t) * 1000);
        qDebug() << "Count tick" << (genTick / 1000000) << (moveTick / 1000000);

    }

    return;

    kc::Board board;
    parseFENString(testFenPerft(), &board);

    myTimer.start();
    int countTotal = genMoveRecur(board, FixedDepth);

//    qint64 t = timer.nsecsElapsed() / 1000000;
//    qDebug() << "Time for multi board" << t << "ms";
    qDebug() << "Total move calculated" << countMate << countCapture << countCheck << countTotal;
    qDebug() << "Count ext" << countMoveExt;

//    return;

    {
        std::map<std::string, int> newDivideCount;
        for (auto it = divideCount.begin(); it != divideCount.end(); it++){
            auto key = it->first;
            auto value = it->second;
            std::string newKey = "";
            newKey += key[0] + 32;
            newKey += key[1];
            newKey += key[3] + 32;
            newKey += key[4];
            newDivideCount[newKey] = value;
        }
        divideCount = newDivideCount;

        QString sample = MULTI_LINE_STRING(
                    b4b3: 1
                    g6g5: 1
                    c7c6: 1
                    d7d6: 1
                    c7c5: 1
                    e6d5: 1
                    b4c3: 1
                    b6a4: 1
                    b6c4: 1
                    b6d5: 1
                    b6c8: 1
                    f6e4: 1
                    f6g4: 1
                    f6d5: 1
                    f6h5: 1
                    f6h7: 1
                    f6g8: 1
                    a6e2: 1
                    a6d3: 1
                    a6c4: 1
                    a6b5: 1
                    a6b7: 1
                    a6c8: 1
                    g7h6: 1
                    g7f8: 1
                    a8b8: 1
                    a8c8: 1
                    a8d8: 1
                    h8h4: 1
                    h8h5: 1
                    h8h6: 1
                    h8h7: 1
                    h8f8: 1
                    h8g8: 1
                    e7c5: 1
                    e7d6: 1
                    e7d8: 1
                    e7f8: 1
                    e8d8: 1
                    e8f8: 1
                    e8g8: 1
                    e8c8: 1
       );



        sample = sample.replace(':', "");
        std::map<std::string, int> sampleDivide;
        auto sp = sample.split(' ');
        for (int i = 0; i < sp.size(); i++){
            if (i % 2 == 0){
                sampleDivide[sp[i].toStdString()] = sp[i + 1].toInt();
            }
        }
        // Compare sample divide count with divide count
        for (auto it = divideCount.begin(); it != divideCount.end(); it++){
            auto key = it->first;
            auto value = it->second;
            if (sampleDivide[key] != value){
                qDebug() << "Error at" << key.c_str() << "expected" << sampleDivide[key] << "but got" << value;
            }
        }
        // Find all contains in sampleDivide but not in divideCount
        for (auto it = sampleDivide.begin(); it != sampleDivide.end(); it++){
            auto key = it->first;
            auto value = it->second;
            if (divideCount.find(key) == divideCount.end()){
                qDebug() << "Error at" << key.c_str() << "expected" << value << "but got" << 0;
            }
        }
    }
}

int kc::genMoveRecur(Board &board, int depth)
{
//     qDebug() << board.getPrintable(FixedDepth - depth).c_str();
//    qint64 startGen = myTimer.nsecsElapsed();
    Move moves[256];
    int count = generateMoveList(board, moves);
    if (depth == 2){
        countMoveExt += count;
    }
    if (count == 0){
        countMate++;
    }
//    qint64 endGen = myTimer.nsecsElapsed();
//    genTick += (endGen - startGen);

    if (depth == 0){
        divideCount[currMove]++;
        return 1;
    }

    BoardState state;

    int total = 0;
    for (int i = 0; i < count; i++){
//        qint64 startMove = myTimer.nsecsElapsed();

        if (depth == FixedDepth){
            currMove = moves[i].getDescription();
//            qDebug() << "Start cal for move" << moves[i].getDescription().c_str();
        }
        board.doMove(moves[i], state);
//        qint64 endMove = myTimer.nsecsElapsed();
//        moveTick += (endMove - startMove);
        int child = genMoveRecur(board, depth - 1);
//        if (depth == FixedDepth){
//            qDebug() << "Ret" << child;
//        }
        total += child;
//        startMove = myTimer.nsecsElapsed();
        board.undoMove(moves[i]);
//        endMove = myTimer.nsecsElapsed();
//        moveTick += (endMove - startMove);
    }
//    free(moves);

    return total;
}



