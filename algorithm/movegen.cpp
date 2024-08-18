#include "movegen.h"
#include "attack.h"
#include <bitset>
#include <QDebug>
#include <QElapsedTimer>
#include "evaluation.h"
#include "util.h"
using namespace kc;


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
    BB mines = board.getMines<color>();
    BB notMines = ~mines;
    BB enemies = board.getEnemies<color>();
    BB occ = board.getOccupancy();
    BB notOcc = ~occ;
    const BB *attackPawns = attack::pawns[color];
    const BB *attackPawnPushes = attack::pawnPushes[color];
    const BB *attackPawnPushes2 = attack::pawnPushes2[color];
    const BB &pawns = board.types[Pawn];
    const BB &knights = board.types[Knight];
    const BB &bishops = board.types[Bishop];
    const BB &rooks = board.types[Rook];
    const BB &queens = board.types[Queen];
    const BB &kings = board.types[King];

    BB ourKing = kings & mines;
    int ourKingIdx = lsb(ourKing);
    BB occWithoutOurKing = occ & (~ourKing); // Trong trường hợp slide attack vẫn có thể 'nhìn' các vị trí sau vua

    BB kingBan = 0;
    BB checkMask = All_BB; // Giá trị checkmask = 0xff.ff. Nếu đang chiếu sẽ bằng giá trị từ quân cờ tấn công đến vua (trừ vua)
    BB pinMaskCross = 0;
    BB pinMaskDiagonal = 0;

    BB enemyPawnAttacks = attack::getPawnAttacks<!color>(enemies & pawns);
    kingBan |= enemyPawnAttacks;
    if (enemyPawnAttacks & ourKing){
        checkMask &= (attack::pawns[color][ourKingIdx] & enemies & pawns);
    }

    BB enemyKnightAttacks = attack::getKnightAttacks(enemies & knights);
    kingBan |= enemyKnightAttacks;
    if (enemyKnightAttacks & ourKing){
        checkMask &= (attack::knights[ourKingIdx] & enemies & knights);
    }

    BB enemyKingAttacks = attack::getKingAttacks(enemies & kings);
    kingBan |= enemyKingAttacks;

    BB enemyBishops = enemies & (bishops | queens);
    while (enemyBishops) {
        int i = popLsb(enemyBishops);
        BB eAttack = attack::getBishopAttacks(i, occWithoutOurKing);
        kingBan |= eAttack;
        if (eAttack & ourKing){
            checkMask &= ((attack::getBishopAttacks(i, occ)
                          & attack::getBishopAttacks(ourKingIdx, occ)) | indexToBB(i));
        }
        BB xrayAttack = attack::getBishopXRay(i, occ);
        if (xrayAttack & ourKing){
            pinMaskDiagonal |= ((xrayAttack & attack::getBishopXRay(ourKingIdx, occ)& (~ourKing)) | indexToBB(i));
        }
    }
    BB enemyRooks = enemies & (rooks | queens);
    while (enemyRooks){
        int i = popLsb(enemyRooks);
        BB eAttack = attack::getRookAttacks(i, occWithoutOurKing);
        kingBan |= eAttack;
        if (eAttack & ourKing){
            checkMask &= ((attack::getRookAttacks(i, occ)
                          & attack::getRookAttacks(ourKingIdx, occ)) | indexToBB(i));
        }
        BB xrayAttack = attack::getRookXRay(i, occ);
        if (xrayAttack & ourKing){
            pinMaskCross |= ((xrayAttack & attack::getRookXRay(ourKingIdx, occ) & (~ourKing)) | indexToBB(i));
        }
    }

    BB notOccShift8ByColor = color == White ? (notOcc << 8) : (notOcc >> 8);
    BB pawnPush2Mask = notOcc & notOccShift8ByColor;
    int count = 0;
    for (int i = 0; i < Square_Count; i++){
        u64 from = indexToBB(i);
        if ((from & mines) == 0)
            continue;
        u64 attack = 0, to = 0;
        if (from & pawns){
            if (pinMaskDiagonal & from){
                attack = attackPawns[i] & enemies & checkMask & pinMaskDiagonal;
            } else if (pinMaskCross & from){
                attack = ((attackPawnPushes[i] & notOcc)
                        | (attackPawnPushes2[i] & pawnPush2Mask)) & checkMask & pinMaskCross;
            } else {
                attack = (attackPawns[i] & enemies)
                       | (attackPawnPushes[i] & notOcc)
                       | (attackPawnPushes2[i] & pawnPush2Mask);
                attack &= checkMask;
            }
            bool enPassantCond = board.state->enPassant != SquareNone // Có trạng thái một tốt vừa được push 2
                    && ((board.state->enPassant / 8) == (i / 8)) // Ô tốt push 2 ở cùng hàng với ô from
                    && ((board.state->enPassant - i) == 1 || (i - board.state->enPassant == 1)) // 2 ô cách nhau 1 đơn vị
                    && ((pinMaskDiagonal & indexToBB(board.state->enPassant)) == 0) // Ô tấn công không bị pin theo đường chéow
                    ;
            if (enPassantCond){
                // Kiểm tra thêm trường hợp ngang hàng tốt có hậu hoặc xe đang tấn công xuyên 2 tốt
                Rank ourKingRank = getRank(Square(ourKingIdx));
                BB ourKingRankBB = rankBB(ourKingRank);

                BB enemieRookOrQueenInRanks = (queens | rooks) & ourKingRankBB & enemies;

                BB enemyRookOrQueen;
                BB occWithout2Pawn = occ & (~indexToBB(board.state->enPassant)) & (~from);
                bool isKingSeenByEnemyRookOrQueen = false;
                while (enemieRookOrQueenInRanks){
                    enemyRookOrQueen = lsbBB(enemieRookOrQueenInRanks);
                    int sqRQ = lsb(enemyRookOrQueen);
                    BB rookQueenAttack = attack::getRookAttacks(sqRQ, occWithout2Pawn);
                    if (rookQueenAttack & ourKing){
                        isKingSeenByEnemyRookOrQueen = true;
                        break;
                    }
                    enemieRookOrQueenInRanks ^= enemyRookOrQueen;
                }

                if (!isKingSeenByEnemyRookOrQueen){
                    moveList[count++] = Move::makeEnpassantMove(i, board.state->enPassantTarget<color>());
                }
            }
            bool promotionCond = color == White ? (i / 8) == 6 : (i / 8) == 1;
            if (promotionCond){
                while (attack){
                    to = lsbBB(attack);
                    moveList[count++] = Move::makePromotionMove(i, lsb(to), Queen);
                    moveList[count++] = Move::makePromotionMove(i, lsb(to), Rook);
                    moveList[count++] = Move::makePromotionMove(i, lsb(to), Bishop);
                    moveList[count++] = Move::makePromotionMove(i, lsb(to), Knight);
                    attack ^= to;
                }
            }
        } else if (from & knights) {
            attack = attack::knights[i] & notMines & checkMask;
            if ((pinMaskCross & from) || (pinMaskDiagonal & from)){
                // Quân mã bị pin sẽ không thể di chuyển đến bất kỳ vị trí nào
                attack = 0;
            }
        } else if (from & bishops) {
            attack = attack::getBishopAttacks(i, occ) & notMines & checkMask;
            if (pinMaskCross & from) {
                attack = 0;
            } else if (pinMaskDiagonal & from){
                attack &= pinMaskDiagonal;
            }
        } else if (from & rooks) {
            attack = attack::getRookAttacks(i, occ) & notMines & checkMask;
            if (pinMaskDiagonal & from) {
                attack = 0;
            } else if (pinMaskCross & from){
                attack &= pinMaskCross;
            }
        } else if (from & queens) {
            if (pinMaskDiagonal & from){
                attack = attack::getBishopAttacks(i, occ) & notMines & checkMask & pinMaskDiagonal;
            } else if (pinMaskCross & from){
                attack = attack::getRookAttacks(i, occ) & notMines & checkMask & pinMaskCross;
            } else {
                attack = attack::getQueenAttacks(i, occ) & notMines & checkMask;
            }
        } else if (from & kings) {
            attack = attack::kings[i] & notMines & (~kingBan);
            if constexpr (color == White) {
                if ((occ & castlingSpace<CastlingWK>()) == 0
                        && (kingBan & castlingKingPath<CastlingWK>()) == 0
                        && (board.state->castlingRights & CastlingWK) ){
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
        while (attack){
            to = lsbBB(attack);
            moveList[count++] = Move::makeNormalMove(i, lsb(to));
            attack ^= to;
        }
    }
    return count;
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

constexpr static int FixedDepth = 3;
int countMate = 0;
int countCapture = 0;
int countCheck = 0;
std::map<std::string, int> divideCount;
std::string currMove;
QElapsedTimer myTimer;
qint64 genTick = 0;
qint64 moveTick = 0;


std::string kc::testFenPerft()
{
//    return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    return "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 2 1"; // Root
//    return "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2R1K2R b Kkq - 3 1"; // A1 c1 81114 (ep: 83263)
}


void kc::testPerft()
{
//    return;
    kc::Board board;
    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
    // rnbqk1nr/pppp1ppp/8/P3p3/1b6/8/1PPPPPPP/RNBQKBNR w KQkq - 1 3
//    parseFENString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &board);
    parseFENString(testFenPerft(), &board);

    QElapsedTimer timer;
    timer.start();
    myTimer.start();
    int countTotal = genMoveRecur(board, FixedDepth);

    qint64 t = timer.nsecsElapsed() / 1000000;
    qDebug() << "Time for multi board" << t << "ms";
    qDebug() << "Total move calculated" << countMate << countCapture << countCheck << countTotal;
    qDebug() << "Count tick" << (genTick / 1000000) << (moveTick / 1000000);

//    return;

    {
//        for (auto it = divideCount.begin(); it != divideCount.end(); it++){
//            qDebug() << it->first.c_str() << it->second;
//        }
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

        QString sample = "a2a3: 94405 b2b3: 81066 g2g3: 77468 d5d6: 79551 a2a4: 90978 g2g4: 75677 g2h3: 82759 d5e6: 97464 c3b1: 84773 c3d1: 84782 c3a4: 91447 c3b5: 81498 e5d3: 77431 e5c4: 77752 e5g4: 79912 e5c6: 83885 e5g6: 83866 e5d7: 93913 e5f7: 88799 d2c1: 83037 d2e3: 90274 d2f4: 84869 d2g5: 87951 d2h6: 82323 e2d1: 74963 e2f1: 88728 e2d3: 85119 e2c4: 84835 e2b5: 79739 e2a6: 69334 a1b1: 83348 a1c1: 83263 a1d1: 79695 h1f1: 81563 h1g1: 84876 f3d3: 83727 f3e3: 92505 f3g3: 94461 f3h3: 98524 f3f4: 90488 f3g4: 92037 f3f5: 104992 f3h5: 95034 f3f6: 77838 e1d1: 79989 e1f1: 77887 e1g1: 86975 e1c1: 79803";

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
    qint64 startGen = myTimer.nsecsElapsed();
    Move moves[256];
    int count = generateMoveList(board, moves);
    if (count == 0){
        countMate++;
    }
    qint64 endGen = myTimer.nsecsElapsed();
    genTick += (endGen - startGen);

    if (depth == 0){
        divideCount[currMove]++;
        return 1;
    }

    BoardState state;

    int total = 0;
    for (int i = 0; i < count; i++){
        qint64 startMove = myTimer.nsecsElapsed();

        if (depth == FixedDepth){
            currMove = moves[i].getDescription();
            qDebug() << "Start cal for move" << moves[i].getDescription().c_str();
        }
        board.doMove(moves[i], state);
        qint64 endMove = myTimer.nsecsElapsed();
        moveTick += (endMove - startMove);
        total += genMoveRecur(board, depth - 1);
        board.undoMove(moves[i]);
    }
//    free(moves);

    return total;
}



