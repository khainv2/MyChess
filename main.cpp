#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <algorithm/attack.h>
#include <algorithm/board.h>
#include <algorithm/movegen.h>
#include <algorithm/util.h>
#include <test.h>


#include "logger.h"

//#include <intrin.h>
#include <QElapsedTimer>

using namespace kc;


#include <iostream>
#include <chrono>

const int NUM_OPERATIONS = 1000000000;  // Số lượng phép toán lớn để thử nghiệm

void testCpuPerformance() {
    // Bắt đầu thời gian
    auto start = std::chrono::high_resolution_clock::now();

    // Số phép toán thực hiện
    long long operationCount = 0;

    // Thực hiện phép toán liên tục trong vòng 10 giây
    while (true) {
        // Thực hiện phép cộng đơn giản
        int a = 1;
        int b = 2;
        int c = a + b;

        operationCount++;

        // Kiểm tra thời gian đã trôi qua
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - start;

        if (elapsed.count() >= 10.0) { // Đoạn mã chạy trong 10 giây
            break;
        }
    }

    std::cout << "Number of operations in 10 seconds: " << operationCount << std::endl;
}


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
    qDebug() << "Time init attack" << (timer.nsecsElapsed() / 1000) << "us";

//    qDebug() << "BIt scan" << kc::bitScanForward(0x11000111000);
//    testCpuPerformance();

//    qDebug() << "King board" << bbToString(board.types[King]).c_str();

    kc::Board board1, board2;
    parseFENString("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 2 1", &board1);
    Move move = Move::makeNormalMove(A1, C1);
    kc::BoardState state;
//    board1.doMove(move, state);
    parseFENString("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2R1K2R b Kkq - 3 1", &board2);


//    board1.compareTo(board2);

    kc::testPerft();
    MainWindow w;
    w.show();

    return a.exec();
}
