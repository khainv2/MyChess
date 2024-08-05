#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <algorithm/attack.h>

#include <intrin.h>
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

    static QString fileTxt[8] = { "A", "B", "C", "D", "E", "F", "G", "H" };
    static QString rankTxt[8] = { "1", "2", "3", "4", "5", "6", "7", "8" };
    for (int i = 0; i < 8; i++){
        QString str;
        for (int j = 0; j < 8; j++){
            str =  (str + "_") + fileTxt[j] + rankTxt[i] + ", ";
        }
        qDebug() << str;
    }

    QElapsedTimer timer;
    timer.start();
    attack::init();
    qDebug() << "Time init attack" << (timer.nsecsElapsed() / 1000) << "us";
    MainWindow w;
    w.show();

    return a.exec();
}
