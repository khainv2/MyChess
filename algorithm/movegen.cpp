#include "movegen.h"
#include "attack.h"
#include <bitset>
#include <QDebug>
#include <QElapsedTimer>
#include "evaluation.h"

using namespace kc;
void kc::generateMoveList(const Board &board, Move *moveList, int &count)
{
    BB mines = board.mines();
    BB notMines = ~mines;
    BB enemies = board.enemies();
    BB occ = board.occupancy();
    BB notOcc = ~occ;
    Color color = board.side;
    BB notOccShift8ByColor = color == White ? (notOcc << 8) : (notOcc >> 8);
    BB pawnPush2Mask = notOcc & notOccShift8ByColor;
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
    int ourKingIdx = bbToSquare(ourKing);
    BB occWithoutOurKing = occ & (~ourKing); // Trong trường hợp slide attack vẫn có thể 'nhìn' các vị trí sau vua
    BB enemyAttackOurKing = 0;
    BB checkMask = All_BB; // Giá trị checkmask = 0xff.ff. Nếu đang chiếu sẽ bằng giá trị từ quân cờ tấn công đến vua (trừ vua)
    for (int i = 0; i < Square_Count; i++){
        u64 from = squareToBB(i);
        if ((from & enemies) == 0)
            continue;
        if (from & pawns){
            BB eAttack = attack::pawns[!color][i];
            enemyAttackOurKing |= eAttack;
            if (eAttack & ourKing){
                checkMask &= from;
            }
        } else if (from & knights){
            BB eAttack = attack::knights[i];
            enemyAttackOurKing |= eAttack;
            if (eAttack & ourKing){
                checkMask &= from;
            }
        } else if (from & bishops){
            BB eAttack = attack::getBishopAttacks(i, occWithoutOurKing);
            enemyAttackOurKing |= eAttack;
            if (eAttack & ourKing){
                checkMask &= ((attack::getBishopAttacks(i, occ)
                              & attack::getBishopAttacks(ourKingIdx, occ)) | from);
            }
        } else if (from & rooks){
            BB eAttack = attack::getRookAttacks(i, occWithoutOurKing);
            enemyAttackOurKing |= eAttack;
            if (eAttack & ourKing){
                checkMask &= ((attack::getRookAttacks(i, occ)
                              & attack::getRookAttacks(ourKingIdx, occ)) | from);
            }
        } else if (from & queens){
            BB eAttack = attack::getQueenAttacks(i, occWithoutOurKing);
            enemyAttackOurKing |= eAttack;
            if (eAttack & ourKing){

                checkMask &= (attack::getQueenAttacks(ourKingIdx, occ)
                              & (attack::rookMagicBitboards[i].mask | attack::bishopMagicBitboards[i].mask));
            }
        } else if (from & kings){
            enemyAttackOurKing |= attack::kings[i];
            // Một quân vua không thể 'chiếu' quân vua khác
        }
    }

    qDebug() << "Print checkmask" << bbToString(checkMask).c_str();

//    if (checkMask)
    bool isCheck = (enemyAttackOurKing & ourKing) != 0;

    count = 0;
    for (int i = 0; i < Square_Count; i++){
        u64 from = squareToBB(i);
        if ((from & mines) == 0)
            continue;
        u64 attack = 0, to = 0;
        if (from & pawns){
            attack = (attackPawns[i] & enemies)
                   | (attackPawnPushes[i] & notOcc)
                   | (attackPawnPushes2[i] & pawnPush2Mask);
            bool enPassantCond = board.enPassant != SquareNone // Có trạng thái một tốt vừa được push 2
                    && ((board.enPassant / 8) == (i / 8)) // Ô tốt push 2 ở cùng hàng với ô from
                    && ((board.enPassant - i) == 1 || (i - board.enPassant == 1)); // 2 ô cách nhau 1 đơn vị
            if (enPassantCond){
                moveList[count++] = Move::makeEnpassantMove(i, color == White ? board.enPassant + 8 : board.enPassant - 8);
            }
            bool promotionCond = color == White ? (i / 8) == 6 : (i / 8) == 1;
            if (promotionCond){
                while (attack){
                    to = lsbBB(attack);
                    moveList[count++] = Move::makePromotionMove(i, bbToSquare(to), Queen);
                    moveList[count++] = Move::makePromotionMove(i, bbToSquare(to), Rook);
                    moveList[count++] = Move::makePromotionMove(i, bbToSquare(to), Bishop);
                    moveList[count++] = Move::makePromotionMove(i, bbToSquare(to), Knight);
                    attack ^= to;
                }
            }
        } else if (from & knights){
            attack = attack::knights[i] & notMines;
        } else if (from & bishops){
            attack = attack::getBishopAttacks(i, occ) & notMines;
        } else if (from & rooks){
            attack = attack::getRookAttacks(i, occ) & notMines;
        } else if (from & queens){
            attack = attack::getQueenAttacks(i, occ) & notMines;
        } else if (from & kings){
            attack = attack::kings[i] & notMines & (~enemyAttackOurKing);
            if (color == White){
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
            moveList[count++] = Move::makeNormalMove(i, bbToSquare(to));
            attack ^= to;
        }
    }
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
        u64 from = squareToBB(i);
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
    int count;
    generateMoveList(board, moves, count);
    std::vector<Move> output;
    for (int i = 0; i < count; i++){
        const auto &move = moves[i];
        if (move.src() == square){
            output.push_back(moves[i]);
        }
    }
    return output;
}

constexpr static int FixedDepth = 6;

void kc::generateMove(const Board &b)
{
    QElapsedTimer timer;
    timer.start();

//    std::vector<Move> moves;
    constexpr int MaxMoveSize = 1000000000;
    Move *moves = reinterpret_cast<Move *>(malloc(sizeof(Move) * MaxMoveSize));
    Board board = b;

    qint64 t1 = timer.nsecsElapsed() / 1000000;

//    qDebug() << "Start init move list";

    int countTotal = 0;
    genMoveRecur(board, moves, countTotal, FixedDepth);

    qint64 t = timer.nsecsElapsed() / 1000000;
    qDebug() << "Time for allocate mem" << t1 << "ms";
    qDebug() << "Time for multi board" << t << "ms";
    qDebug() << "Total move calculated" << countTotal;

}

void kc::genMoveRecur(const Board &board, Move *ptr, int &totalCount, int depth)
{
//    qDebug() << board.getPrintable(FixedDepth - depth).c_str();

    if (depth == 0){
        return;
    }

    int count;
    auto moves = ptr + totalCount;
    generateMoveList(board, moves, count);
    totalCount += count;

    for (int i = 0; i < count; i++){
        Board newBoard = board;
        newBoard.doMove(moves[i]);
        genMoveRecur(newBoard, ptr, totalCount, depth - 1);
    }
}
































