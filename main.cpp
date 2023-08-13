#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <algorithm/bitwise.h>
#include <algorithm/attack.h>

#include <intrin.h>

using namespace kchess;
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    attack::init();
    u64 b = 0x00100;
    qDebug() << QString::number(b, 2)
             << QString::number(b >> 2, 2);
    qDebug() << bbToSquare(b);
    qDebug() << bbToSquare(b >> 1);
    qDebug() << bbToSquare(b >> 3);
    unsigned long idx;
    _BitScanForward(&idx, b);
    qDebug() << idx;
    _BitScanForward(&idx, b >> 1);
    qDebug() << idx;
    _BitScanForward(&idx, b >> 2);
    qDebug() << idx;
    _BitScanForward(&idx, b >> 3);
    qDebug() << idx;
//    for (int i = 0; i < 64; i++){
//        for (int j = 0; j < 64; j++){
//            auto m = makeMovePromotion(i, j, Knight);
//            auto src = getSourceFromMove(m);
//            auto dst = getDestinationFromMove(m);
//            auto moveType = getMoveType(m);
//            auto piece = getPiecePromotion(m);
//            if (i != src || j != dst || moveType != Promotion || piece != Knight){
//                qDebug() << "Move err" << i << j << src << dst << piece << moveType;
//            }
//        }
//    }
    for (int i = 0; i < 64; i++){
        u64 sqbb = squareToBB(i);
//        u64 mask =
    }


    MainWindow w;
    w.show();

    return a.exec();
}
