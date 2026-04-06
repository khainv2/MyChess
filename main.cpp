#include "mainwindow.h"
#include "testviewerdialog.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <algorithm/attack.h>
#include <algorithm/board.h>
#include <algorithm/movegen.h>
#include <algorithm/util.h>
#include <algorithm/perft.h>
#include <algorithm/evaluation.h>
#include <algorithm/tt.h>

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
    MoveGen::init();
    eval::init();
    kc::zobrist::init();

//    Perft::testPerft();

    // Check for --test flag or auto-detect CSV
    QStringList args = a.arguments();
    bool testMode = !args.contains("--no-test");
    qDebug() << "Test mode" << testMode;

    // Auto-detect test CSV relative to executable
    QString appDir = QCoreApplication::applicationDirPath();
    // Try common locations for the CSV
    QStringList csvSearchPaths = {
        appDir + "/../../tests/wac_results.csv",       // from _build/debug/
        appDir + "/../tests/wac_results.csv",
        appDir + "/tests/wac_results.csv",
        "tests/wac_results.csv",                       // relative to CWD
        "G:/Projects/MyChess/tests/wac_results.csv"
    };

    QString engineCsv;
    for (const auto &p : csvSearchPaths) {
        if (QFile::exists(p)) {
            engineCsv = QFileInfo(p).absoluteFilePath();
            break;
        }
    }

    if (testMode && !engineCsv.isEmpty()) {
        auto *viewer = new TestViewerDialog(engineCsv);
        viewer->setAttribute(Qt::WA_DeleteOnClose);
        viewer->show();
    } else {
        auto *w = new MainWindow;
        w->show();
    }

    return a.exec();
}
