#include "perft.h"
#include "util.h"
#include "movegen.h"
#include <QElapsedTimer>

#define TEST_PERFT

using namespace kc;


std::string Perft::testFenPerft()
{
    return "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 5 0";
}

#define MULTI_LINE_STRING(a) #a
constexpr static int FixedDepth = 6;
int countMate = 0;
int countCapture = 0;
int countCheck = 0;
std::map<std::string, int> divideCount;
std::string currMove;
QElapsedTimer myTimer;
qint64 genTick = 0;
qint64 moveTick = 0;
int countMoveExt = 0;

void kc::Perft::testPerft()
{
//    return;
    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
    // rnbqk1nr/pppp1ppp/8/P3p3/1b6/8/1PPPPPPP/RNBQKBNR w KQkq - 1 3
//    parseFENString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &board);
    u64 totalCount = 0;
    auto funcTest = [&](const std::string fen, int depth, u64 expectedTotalMove){
        QElapsedTimer timer;
        kc::Board board;
        parseFENString(fen, &board);
        timer.start();
        u64 moveCount = genMoveRecur(board, depth);
        if (moveCount != expectedTotalMove){
            qWarning() << "Wrong val when test perft";
            qWarning() << fen.c_str() << depth << moveCount << "expected" << expectedTotalMove;
        } else {
            qDebug() << "Test perft done" << fen.c_str() << "on" << (timer.nsecsElapsed() / 1000000) << "ms, num count" << moveCount;
        }
        totalCount += moveCount;
    };
    bool test = false;
#ifdef TEST_PERFT
    test = true;
#endif


//    funcTest("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039);
    if (test){
        qDebug() << "Start test perft";
        QElapsedTimer timer;
        timer.start();
//        funcTest("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, 3195901860ULL);
        funcTest("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, 119060324);
//        funcTest("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 193690690);
//        funcTest("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 5, 164075551);
//        funcTest("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 10", 7, 178633661);
//        funcTest("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5, 89941194);

        auto t = timer.nsecsElapsed() / 1000000;
        qDebug() << "Time for multi board" << t << "ms";
        qDebug() << "Count node per sec=" << ((totalCount / t) * 1000);
        qDebug() << "Count tick" << (genTick / 1000000) << (moveTick / 1000000);

    }
#ifdef TEST_PERFT
    return;
#endif

    kc::Board board;
    parseFENString(testFenPerft(), &board);

    myTimer.start();
    u64 countTotal = genMoveRecur(board, FixedDepth);

    qDebug() << "Total move calculated" << countMate << countCapture << countCheck << countTotal;
    qDebug() << "Count ext" << countMoveExt;

//    return;

    {
        std::map<std::string, int> newDivideCount;
        for (auto it = divideCount.begin(); it != divideCount.end(); it++){
            auto key = it->first;
            auto value = it->second;
            std::string newKey = "";
            newKey += key[0] + 32;
            newKey += key[1];
            newKey += key[3] + 32;
            newKey += key[4];
            newDivideCount[newKey] = value;
        }
        divideCount = newDivideCount;

        QString sample = MULTI_LINE_STRING(
                    b4b3: 1919
                    g6g5: 2090
                    c7c6: 2222
        );

        sample = sample.replace(':', "");
        std::map<std::string, int> sampleDivide;
        auto sp = sample.split(' ');
        for (int i = 0; i < sp.size(); i++){
            if (i % 2 == 0){
                sampleDivide[sp[i].toStdString()] = sp[i + 1].toInt();
            }
        }
        // Compare sample divide count with divide count
        for (auto it = divideCount.begin(); it != divideCount.end(); it++){
            auto key = it->first;
            auto value = it->second;
            if (sampleDivide[key] != value){
                qDebug() << "Error at" << key.c_str() << "expected" << sampleDivide[key] << "but got" << value;
            }
        }
        // Find all contains in sampleDivide but not in divideCount
        for (auto it = sampleDivide.begin(); it != sampleDivide.end(); it++){
            auto key = it->first;
            auto value = it->second;
            if (divideCount.find(key) == divideCount.end()){
                qDebug() << "Error at" << key.c_str() << "expected" << value << "but got" << 0;
            }
        }
    }
}

//#define CountTickGenAndMove
Move moves[10][256];
u64 kc::Perft::genMoveRecur(Board &board, int depth)
{
    auto movePtr = moves[depth];
    if (depth == 1){
        int count1 = MoveGen::instance->countMoveList(board);
        if (count1 == 0){
            countMate++;
        }
        return count1;
    } else {
//        qDebug() << "Count";
        int count = MoveGen::instance->genMoveList(board, moves[depth]);
//        qDebug() << "Count first..." << count;
        if (count == 0){
            countMate++;
        }

        if (depth == 1){
            return count;
        }

        BoardState state;

        u64 total = 0;
        for (int i = 0; i < count; i++){
//            if (depth == FixedDepth){
//                currMove = movePtr[i].getDescription();
//                qDebug() << "Start cal for move" << movePtr[i].getDescription().c_str();
//            }
            board.doMove(movePtr[i], state);
            total += genMoveRecur(board, depth - 1);
            board.undoMove(movePtr[i]);
        }
        return total;
    }
}
