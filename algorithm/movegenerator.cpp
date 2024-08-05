#include "movegenerator.h"
#include "attack.h"
#include <bitset>
#include <QDebug>
#include <QElapsedTimer>
using namespace kchess;
void kchess::generateMoveList(const ChessBoard &board, Move *moveList, int &count)
{
    u64 mines = board.mines();
    u64 notMines = ~mines;
    u64 enemies = board.enemies();
    u64 occ = board.occupancy();
    u64 notOcc = ~occ;

    Color color = getColorActive(board.state);
    u64 notOccShift8ByColor = color == White ? (notOcc << 8) : (notOcc >> 8);
    u64 pawnPush2Mask = notOcc & notOccShift8ByColor;
    const u64 *attackPawns = attack::pawns[color];
    const u64 *attackPawnPushes = attack::pawnPushes[color];
    const u64 *attackPawnPushes2 = attack::pawnPushes2[color];
    count = 0;
    for (int i = 0; i < 64; i++){
        u64 from = squareToBB(i);
        if ((from & mines) == 0)
            continue;
        u64 attack = 0, to = 0;
        if (from & board.pawns){
            attack = (attackPawns[i] & enemies)
                   | (attackPawnPushes[i] & notOcc)
                   | (attackPawnPushes2[i] & pawnPush2Mask);
        } else if (from & board.knights){
            attack = attack::knights[i] & notMines;
        } else if (from & board.bishops){
            attack = attack::getBishopAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.rooks){
            attack = attack::getRookAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.queens){
            attack = attack::getQueenAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.kings){
            attack = attack::kings[i] & notMines;
            if (color == White){
                if ((occ & CastlingWhiteQueenSideSpace) == 0 && getWhiteOOO(board.state) == 0){
                    moveList[count++] = makeMoveCastling(i, _C1);
                }
                if ((occ & CastlingWhiteKingSideSpace) == 0 && getWhiteOO(board.state) == 0){
                    moveList[count++] = makeMoveCastling(i, _G1);
                }
            } else {
                if ((occ & CastlingBlackQueenSideSpace) == 0 && getBlackOOO(board.state) == 0){
                    moveList[count++] = makeMoveCastling(i, _C8);
                }
                if ((occ & CastlingBlackKingSideSpace) == 0 && getBlackOO(board.state) == 0){
                    moveList[count++] = makeMoveCastling(i, _G8);
                }
            }
        }
        while (attack){
            to = lsbBB(attack);
            moveList[count++] = makeMove(i, bbToSquare(to));
            attack ^= to;
        }
    }
}

int kchess::countMoveList(const ChessBoard &board, Color color)
{
    u64 mines = color == White ? board.whites : board.blacks;
    u64 notMines = ~mines;
    u64 enemies = color == White ? board.blacks : board.whites;
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
        if (from & board.pawns){
            attack = (attackPawns[i] & enemies)
                   | (attackPawnPushes[i] & notOcc)
                   | (attackPawnPushes2[i] & pawnPush2Mask);
        } else if (from & board.knights){
            attack = attack::knights[i] & notMines;
        } else if (from & board.bishops){
            attack = attack::getBishopAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.rooks){
            attack = attack::getRookAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.queens){
            attack = attack::getQueenAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.kings){
            attack = attack::kings[i] & notMines;
        }
        count += popCount(attack);
    }
    return count;
}

Bitboard kchess::getMobility(const ChessBoard &board, Square square){
    Move moves[256];
    int count;
    QElapsedTimer timer;
    timer.start();
    generateMoveList(board, moves, count);
    qDebug() << "Move list" << count << countMoveList(board, White) << countMoveList(board, Black);
    qDebug() << "Time to gen all move list" << timer.nsecsElapsed();
    u64 mobility = 0;
    for (int i = 0; i < count; i++){
        if (square == getSourceFromMove(moves[i]))
            mobility |= (1ULL << getDestinationFromMove(moves[i]));
    }
    return mobility;
}

