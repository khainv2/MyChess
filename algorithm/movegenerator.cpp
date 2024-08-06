#include "movegenerator.h"
#include "attack.h"
#include <bitset>
#include <QDebug>
#include <QElapsedTimer>
#include "evaluation.h"

using namespace kchess;
void kchess::generateMoveList(const ChessBoard &board, Move *moveList, int &count)
{
    QElapsedTimer timer;
    timer.start();
    u64 t1 = timer.nsecsElapsed();

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

    const auto &pawns = board.pieces[Piece::Pawn];
    const auto &knights = board.pieces[Piece::Knight];
    const auto &bishops = board.pieces[Piece::Bishop];
    const auto &rooks = board.pieces[Piece::Rook];
    const auto &queens = board.pieces[Piece::Queen];
    const auto &kings = board.pieces[Piece::King];

    u64 t2 = timer.nsecsElapsed();

    count = 0;
    for (int i = 0; i < Square_NB; i++){
        u64 from = squareToBB(i);
        if ((from & mines) == 0)
            continue;
        u64 attack = 0, to = 0;
        if (from & pawns){
            attack = (attackPawns[i] & enemies)
                   | (attackPawnPushes[i] & notOcc)
                   | (attackPawnPushes2[i] & pawnPush2Mask);
        } else if (from & knights){
            attack = attack::knights[i] & notMines;
        } else if (from & bishops){
            attack = attack::getBishopAttacks(i, occ) & notMines;
        } else if (from & rooks){
            attack = attack::getRookAttacks(i, occ) & notMines;
        } else if (from & queens){
            attack = attack::getQueenAttacks(i, occ) & notMines;
        } else if (from & kings){
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

    u64 t3 = timer.nsecsElapsed();

//    qDebug() << "Move list" << count;
//    qDebug() << "Time cal" << t1 << t2 << t3;
}

int kchess::countMoveList(const ChessBoard &board, Color color)
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
        if (from & board.pieces[Piece::Pawn]){
            attack = (attackPawns[i] & enemies)
                   | (attackPawnPushes[i] & notOcc)
                   | (attackPawnPushes2[i] & pawnPush2Mask);
        } else if (from & board.pieces[Piece::Knight]){
            attack = attack::knights[i] & notMines;
        } else if (from & board.pieces[Piece::Bishop]){
            attack = attack::getBishopAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.pieces[Piece::Rook]){
            attack = attack::getRookAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.pieces[Piece::Queen]){
            attack = attack::getQueenAttacks(i, board.occupancy()) & notMines;
        } else if (from & board.pieces[Piece::King]){
            attack = attack::kings[i] & notMines;
        }
        count += popCount(attack);
    }
    return count;
}

Bitboard kchess::getMobility(const ChessBoard &board, Square square){
    Move moves[256];
    int count;
    generateMoveList(board, moves, count);
//    qDebug() << "Move list" << count << countMoveList(board, White) << countMoveList(board, Black);
    u64 mobility = 0;
    for (int i = 0; i < count; i++){
        if (square == getSourceFromMove(moves[i]))
            mobility |= (1ULL << getDestinationFromMove(moves[i]));
    }
    return mobility;
}


void kchess::generateMove(const ChessBoard &b)
{
    std::vector<Move> moves;
    moves.resize(1000000);
    std::vector<int> scores;
    scores.resize(moves.size());
    ChessBoard board = b;

//    qDebug() << "Start init move list";
    QElapsedTimer timer;
    timer.start();

    Move *movePtr = moves.data();
    int *scorePtr = scores.data();

    int countTotal = 0;
    int count;
    generateMoveList(board, movePtr, count);
    countTotal += count;

    qDebug() << board.getPrintable().c_str();

    int maxScore = -1000000;
    std::string boardAtMax;
    int minScore = 1000000;
    std::string boardAtMin;

    for (int i = 0; i < count; i++){
        ChessBoard newBoard = board;
        newBoard.doMove(movePtr[i]);
        int ncount;
        generateMoveList(newBoard, movePtr + countTotal, ncount);
        for (int j = 0; j < ncount; j++){
            ChessBoard boardJ = newBoard;
            boardJ.doMove((movePtr + countTotal)[j]);
//            int score = eval::estimate(boardJ);
//            if (score > maxScore){
//                maxScore = score;
//                boardAtMax = boardJ.getPrintable();
//                qDebug() << "Max score" << maxScore;
//            }
//            if (score < minScore){
//                minScore = score;
//                boardAtMin = boardJ.getPrintable();
//                qDebug() << "Min score" << minScore;
//            }

        }
        countTotal += ncount;
    }

    qint64 t = timer.nsecsElapsed();
    qDebug() << "Time for multi board" << t;
    qDebug() << "Total move calculated" << countTotal;
    qDebug() << "Max score" << maxScore;
    qDebug() << "Board at max" << boardAtMax.c_str();
    qDebug() << "Min score" << minScore;
    qDebug() << "Board at min" << boardAtMin.c_str();

}

































