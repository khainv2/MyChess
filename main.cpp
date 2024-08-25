#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <algorithm/attack.h>
#include <algorithm/board.h>
#include <algorithm/movegen.h>
#include <algorithm/util.h>
#include <test.h>
#include "logger.h"
#include <QElapsedTimer>
using namespace kc;

#include <iostream>
#include <chrono>


int main(int argc, char *argv[])
{
        QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    #if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    #if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    #endif
    #endif

    QApplication a(argc, argv);

    QElapsedTimer timer;
    timer.start();
    logger::init();
    attack::init();

//    {

//        int index = 22;
//        qDebug() << bbToString(indexToBB(index)).c_str();
//        qDebug() << bbToString(getLeftDiag(index)).c_str();
//        qDebug() << bbToString(getRightDiag(index)).c_str();
//    }

//    {

//        int index = 55;
//        qDebug() << bbToString(indexToBB(index)).c_str();
//        qDebug() << bbToString(getLeftDiag(index)).c_str();
//        qDebug() << bbToString(getRightDiag(index)).c_str();
//    }
//    kc::testPerft();

//    MainWindow w;
//    w.show();

    return a.exec();
}
