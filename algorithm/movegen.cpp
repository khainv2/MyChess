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
            bool enPassantCond = board.enPassant != SquareNone // Có trạng thái một tốt vừa được push 2
                    && ((board.enPassant / 8) == (i / 8)) // Ô tốt push 2 ở cùng hàng với ô from
                    && ((board.enPassant - i) == 1 || (i - board.enPassant == 1)) // 2 ô cách nhau 1 đơn vị
                    && ((pinMaskDiagonal & indexToBB(board.enPassant)) == 0) // Ô tấn công không bị pin theo đường chéow
                    ;
            if (enPassantCond){
                // Kiểm tra thêm trường hợp ngang hàng tốt có hậu hoặc xe đang tấn công xuyên 2 tốt
                Rank ourKingRank = getRank(Square(ourKingIdx));
                BB ourKingRankBB = rankBB(ourKingRank);

                BB enemieRookOrQueenInRanks = (queens | rooks) & ourKingRankBB & enemies;

                BB enemyRookOrQueen;
                BB occWithout2Pawn = occ & (~indexToBB(board.enPassant)) & (~from);
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
                    moveList[count++] = Move::makeEnpassantMove(i, board.enPassantTarget(color));
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
            if (color == White) {
                if ((occ & CastlingWOOSpace) == 0 && board.whiteOO){
                    moveList[count++] = Move::makeCastlingMove(i, CastlingWKOO);
                }
                if ((occ & CastlingWOOOSpace) == 0 && board.whiteOOO){
                    moveList[count++] = Move::makeCastlingMove(i, CastlingWKOOO);
                }
            } else {
                if ((occ & CastlingBOOSpace) == 0 && board.blackOO){
                    moveList[count++] = Move::makeCastlingMove(i, CastlingBKOO);
                }
                if ((occ & CastlingBOOOSpace) == 0 && board.blackOOO){
                    moveList[count++] = Move::makeCastlingMove(i, CastlingBKOOO);
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


int kc::countMoveList(const Board &board, Color color)
{
    u64 mines = board.colors[color];
    u64 notMines = ~mines;
    u64 enemies = board.colors[1 - color];
    u64 occ = board.occupancy();
    u64 notOcc = ~occ;

    u64 notOccShift8ByColor = color == White ? (notOcc << 8) : (notOcc >> 8);
    u64 pawnPush2Mask = notOcc & notOccShift8ByColor;
    const u64 *attackPawns = attack::pawns[color];
    const u64 *attackPawnPushes = attack::pawnPushes[color];
    const u64 *attackPawnPushes2 = attack::pawnPushes2[color];
    int count = 0;

    for (int i = 0; i < 64; i++){
        u64 from = indexToBB(i);
        if ((from & mines) == 0)
            continue;
        u64 attack = 0;
        if (from & board.types[PieceType::Pawn]){
            attack = (attackPawns[i] & enemies)
                   | (attackPawnPushes[i] & notOcc)
                   | (attackPawnPushes2[i] & pawnPush2Mask);
        } else if (from & board.types[PieceType::Knight]){
            attack = attack::knights[i] & notMines;
        } else if (from & board.types[PieceType::Bishop]){
            attack = attack::getBishopAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.types[PieceType::Rook]){
            attack = attack::getRookAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.types[PieceType::Queen]){
            attack = attack::getQueenAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.types[PieceType::King]){
            attack = attack::kings[i] & notMines;
        }
        count += popCount(attack);
    }
    return count;
}

std::vector<Move> kc::getMoveListForSquare(const Board &board, Square square){
    Move moves[256];
    int count = generateMoveList(board, moves);
    std::vector<Move> output;
    for (int i = 0; i < count; i++){
        const auto &move = moves[i];
        if (move.src() == square){
            output.push_back(moves[i]);
        }
    }
    return output;
}

constexpr static int FixedDepth = 5;
int countMate = 0;
int countCapture = 0;
int countCheck = 0;
std::map<std::string, int> divideCount;
std::string currMove;

QElapsedTimer myTimer;
qint64 genTick = 0;
qint64 moveTick = 0;
void kc::testPerft()
{
//    return;
    kc::Board board;
    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
    // rnbqk1nr/pppp1ppp/8/P3p3/1b6/8/1PPPPPPP/RNBQKBNR w KQkq - 1 3
    parseFENString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &board);

    QElapsedTimer timer;
    timer.start();

//    std::vector<Move> moves;
//    constexpr int MaxMoveSize = 1000000000;



//    qDebug() << "Start init move list";

    myTimer.start();
    int countTotal = genMoveRecur(board, FixedDepth);

    qint64 t = timer.nsecsElapsed() / 1000000;
    qDebug() << "Time for multi board" << t << "ms";
    qDebug() << "Total move calculated" << countMate << countCapture << countCheck << countTotal;

    qDebug() << "Count tick" << (genTick / 1000000) << (moveTick / 1000000);

    return;



    for (auto it = divideCount.begin(); it != divideCount.end(); it++){
        qDebug() << it->first.c_str() << it->second;
    }

    // COnvert divide count keys: A1>A2 to a1a2
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


    QString sample = "a2a3: 22 b2b3: 22 c2c3: 22 d2d3: 22 f2f3: 22 g2g3: 22 h2h3: 22 a2a4: 22 b2b4: 22 c2c4: 22 d2d4: 23 f2f4: 23 h2h4: 22 b1a3: 22 b1c3: 22 g1e2: 22 g1f3: 22 g1h3: 22 f1e2: 22 f1d3: 22 f1c4: 22 f1b5: 21 f1a6: 21 g4d1: 23 g4e2: 23 g4f3: 22 g4g3: 23 g4h3: 22 g4f4: 23 g4h4: 6 g4f5: 20 g4g5: 5 g4h5: 22 g4e6: 3 g4g6: 20 g4d7: 5 g4g7: 19 e1d1: 22 e1e2: 22";

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

}

int kc::genMoveRecur(const Board &board, int depth)
{
    // qDebug() << board.getPrintable(FixedDepth - depth).c_str();
    qint64 startGen = myTimer.nsecsElapsed();
    Move *moves = reinterpret_cast<Move *>(malloc(sizeof(Move) * 256));
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

    int total = 0;
    for (int i = 0; i < count; i++){
        qint64 startMove = myTimer.nsecsElapsed();
        Board newBoard = board;
        if (depth == FixedDepth){
            currMove = moves[i].getDescription();
            qDebug() << "Start cal for move" << moves[i].getDescription().c_str();
        }
        countCapture += newBoard.doMove(moves[i]);
        qint64 endMove = myTimer.nsecsElapsed();
        moveTick += (endMove - startMove);
        total += genMoveRecur(newBoard, depth - 1);
    }
    free(moves);

    return total;
}































