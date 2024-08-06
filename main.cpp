#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <algorithm/attack.h>
#include <algorithm/chessboard.h>
#include <algorithm/movegenerator.h>
#include <algorithm/util.h>
#include <test.h>

//#include <intrin.h>
#include <QElapsedTimer>

using namespace kchess;

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
    attack::init();
    qDebug() << "Time init attack" << (timer.nsecsElapsed() / 1000) << "us";

    kchess::ChessBoard board;
    parseFENString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &board);




    kchess::generateMove(board);
    MainWindow w;
    w.show();

    return a.exec();
}
