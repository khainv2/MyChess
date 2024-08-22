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

//    kc::testPerft();
    MainWindow w;
    w.show();

    return a.exec();
}
