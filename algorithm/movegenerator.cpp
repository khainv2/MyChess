#include "movegenerator.h"
#include "attack.h"
#include <bitset>
#include <QDebug>
#include <QElapsedTimer>
using namespace kchess;
void kchess::generateMoveList(const ChessBoard &board, Move *moveList, int &count)
{
    u64 mines = board.mines();
    u64 _mines = ~board.mines();
    u64 enemies = board.enemies();
    u64 _all = ~board.all();
    Color color = getColorActive(board.state);
    count = 0;

    for (int i = 0; i < 64; i++){
        u64 from = squareToBB(i);
        if ((from & mines) == 0)
            continue;
        u64 attack = 0, to = 0;

        if (from & board.pawns){
            attack = (attack::pawns[color][i] & enemies)
                   | (attack::pawnsOnMove[color][i] & _all)
                   | (attack::pawnsOnMove2[color][i] & _all );
        } else if (from & board.knights){
            attack = attack::knights[i] & _mines;
        } else if (from & board.bishops){
            attack = attack::getBishopAttacks(i, board.all()) & _mines;
        } else if (from & board.rooks){
            attack = attack::getRookAttacks(i, board.all()) & _mines;
        } else if (from & board.queens){
            attack = attack::getQueenAttacks(i, board.all()) & _mines;
        } else if (from & board.kings){
            attack = attack::kings[i] & _mines;
        }

        while (attack){
            to = lsbBB(attack);
            moveList[count++] = makeMove(i, bbToSquare(to));
            attack ^= to;
        }
    }
}

Bitboard kchess::getMobility(const ChessBoard &board, Square square){
    Move moves[256];
    int count;
    QElapsedTimer timer;
    timer.start();
    generateMoveList(board, moves, count);
    qDebug() << "Time to gen all move list" << timer.nsecsElapsed();
    u64 mobility = 0;
    for (int i = 0; i < count; i++){
        if (square == getSourceFromMove(moves[i]))
            mobility |= (1ULL << getDestinationFromMove(moves[i]));
    }
    return mobility;

//    Bitboard sqbb = squareToBB(square);
//    auto enemies = board.enemies();
//    if (sqbb & board.pawns){
//        return (attack::pawns[board.state & 1][square] & board.enemies())
//              | (attack::pawnsOnMove[board.state & 1][square] & ~board.all());
////              |;
//    } else if (sqbb & board.kings){
//        return attack::kings[square] & (~board.mines());
//    } else if (sqbb & board.rooks){
//        return attack::getRookAttacks(square, board.all()) & (~board.mines());
//    } else if (sqbb & board.bishops){
//        return attack::getBishopAttacks(square, board.all()) & (~board.mines());
//    } else if (sqbb & board.knights){
//        return attack::knights[square] & (~board.mines());
//    }
//    return 0;
}
