/**
 * Console-only WAC test runner (no GUI).
 * Build with MyChessConsole.pro
 * Usage: ./MyChessConsole [--limit N] [--time N] [--threads N] [--epd path] [--csv path]
 *   --time N    : giới hạn N ms mỗi nước (mặc định: fixed depth 9)
 *   --threads N : số thread chạy song song (mặc định: 1, tối đa 20)
 */
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QRegularExpression>

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <memory>
#include <functional>
#include <process.h>
#include <windows.h>

#include "algorithm/attack.h"
#include "algorithm/board.h"
#include "algorithm/movegen.h"
#include "algorithm/evaluation.h"
#include "algorithm/engine.h"
#include "algorithm/util.h"
#include "algorithm/tt.h"

using namespace kc;

// ── helpers ──────────────────────────────────────────────────────────
static QString squareToUci(int sq) {
    char file = 'a' + (sq % 8);
    char rank = '1' + (sq / 8);
    return QString("%1%2").arg(file).arg(rank);
}

static QString moveToUci(Move move) {
    QString uci = squareToUci(move.src()) + squareToUci(move.dst());
    if (move.is<Move::Promotion>() || move.is<Move::CapturePromotion>()) {
        PieceType pt = move.getPromotionPieceType();
        const char promo[] = {0, 0, 'n', 'b', 'r', 'q', 0};
        if (pt >= Knight && pt <= Queen) uci += promo[pt];
    }
    return uci;
}

struct EpdEntry {
    QString id, fen;
    QStringList bmSan;
};

static bool matchSan(const QString &san, Move move, const Board &board) {
    int src = move.src(), dst = move.dst();
    int srcFile = src % 8, srcRank = src / 8;
    int dstFile = dst % 8, dstRank = dst / 8;
    Piece piece = board.pieces[src];
    PieceType pt = pieceToType(piece);

    QString s = san;
    s.remove('+'); s.remove('#');

    if (s == "O-O")   return move.is<Move::KingCastling>();
    if (s == "O-O-O") return move.is<Move::QueenCastling>();

    PieceType expectedPt = Pawn;
    int sanIdx = 0;
    if (!s.isEmpty() && s[0].isUpper() && s[0] != 'O') {
        QChar ch = s[0];
        if      (ch == 'N') expectedPt = Knight;
        else if (ch == 'B') expectedPt = Bishop;
        else if (ch == 'R') expectedPt = Rook;
        else if (ch == 'Q') expectedPt = Queen;
        else if (ch == 'K') expectedPt = King;
        sanIdx = 1;
    }
    if (pt != expectedPt) return false;

    if (s.contains('=')) {
        if (!move.is<Move::Promotion>() && !move.is<Move::CapturePromotion>()) return false;
        int eqIdx = s.indexOf('=');
        QChar promoCh = s[eqIdx + 1];
        PieceType promoPt = Pawn;
        if      (promoCh == 'N') promoPt = Knight;
        else if (promoCh == 'B') promoPt = Bishop;
        else if (promoCh == 'R') promoPt = Rook;
        else if (promoCh == 'Q') promoPt = Queen;
        if (move.getPromotionPieceType() != promoPt) return false;
        s = s.left(eqIdx);
    }

    QString remainder = s.mid(sanIdx);
    remainder.remove('x');
    if (remainder.size() < 2) return false;

    QChar dstFileChar = remainder[remainder.size() - 2];
    QChar dstRankChar = remainder[remainder.size() - 1];
    if (dstFileChar < 'a' || dstFileChar > 'h') return false;
    if (dstRankChar < '1' || dstRankChar > '8') return false;
    if (dstFile != (dstFileChar.toLatin1() - 'a')) return false;
    if (dstRank != (dstRankChar.toLatin1() - '1')) return false;

    QString disambig = remainder.left(remainder.size() - 2);
    for (QChar c : disambig) {
        if (c >= 'a' && c <= 'h') { if (srcFile != (c.toLatin1() - 'a')) return false; }
        else if (c >= '1' && c <= '8') { if (srcRank != (c.toLatin1() - '1')) return false; }
    }
    return true;
}

static QVector<EpdEntry> parseEpd(const QString &path) {
    QVector<EpdEntry> entries;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return entries;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(' ');
        if (parts.size() < 5) continue;
        EpdEntry e;
        e.fen = parts.mid(0, 4).join(' ');
        QString ops = parts.mid(4).join(' ');
        QRegularExpression bmRe("bm\\s+([^;]+);");
        auto bmMatch = bmRe.match(ops);
        if (bmMatch.hasMatch())
            for (auto &m : bmMatch.captured(1).split(' ', QString::SkipEmptyParts))
                e.bmSan.append(m.trimmed());
        QRegularExpression idRe("id\\s+\"([^\"]+)\"");
        auto idMatch = idRe.match(ops);
        e.id = idMatch.hasMatch() ? idMatch.captured(1) : "?";
        entries.append(e);
    }
    return entries;
}

// Kết quả cho mỗi bài test (pre-allocated, thread ghi vào slot riêng)
struct TestResult {
    QString gotUci;
    QString pvStr;
    QStringList expectedUciList;
    int depth = 0;
    int score = 0;
    int pvCount = 0;
    qint64 elapsedMs = 0;
    bool ok = false;
};

// ── main ─────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Defaults
    QString epdPath = "tests/wac.epd";
    QString csvPath = "wac_console_results.csv";
    int limit = 0;          // 0 = all
    int startFrom = 0;      // Bỏ qua N bài đầu
    int timePerMove = 0;    // 0 = fixed depth, >0 = ms per move
    int numThreads = 1;     // Số thread chạy song song

    QStringList args = app.arguments();
    for (int i = 1; i < args.size(); i++) {
        if (args[i] == "--limit" && i + 1 < args.size())
            limit = args[++i].toInt();
        else if (args[i] == "--start" && i + 1 < args.size())
            startFrom = args[++i].toInt();
        else if (args[i] == "--time" && i + 1 < args.size())
            timePerMove = args[++i].toInt();
        else if (args[i] == "--threads" && i + 1 < args.size())
            numThreads = qBound(1, args[++i].toInt(), 20);
        else if (args[i] == "--epd" && i + 1 < args.size())
            epdPath = args[++i];
        else if (args[i] == "--csv" && i + 1 < args.size())
            csvPath = args[++i];
    }

    // Init engine (read-only globals, chỉ cần gọi 1 lần)
    attack::init();
    MoveGen::init();
    eval::init();
    kc::zobrist::init();

    auto entries = parseEpd(epdPath);
    if (entries.isEmpty()) {
        qCritical() << "Cannot load EPD:" << epdPath;
        return 1;
    }

    // Áp dụng startFrom: bỏ N bài đầu
    if (startFrom > 0 && startFrom < entries.size()) {
        entries = entries.mid(startFrom);
    }

    int total = (limit > 0 && limit < entries.size()) ? limit : entries.size();

    // Pre-allocate kết quả (mỗi slot được ghi bởi đúng 1 thread, không cần lock)
    std::vector<TestResult> results(total);

    // Atomic counter làm work queue
    std::atomic<int> nextTask{0};
    // Đếm số bài hoàn thành (để in progress)
    std::atomic<int> doneCount{0};
    // Mutex chỉ dùng cho console output
    std::mutex printMtx;

    if (timePerMove > 0) {
        qDebug().noquote() << QString("Running %1 WAC positions (time: %2ms/move, threads: %3)...")
            .arg(total).arg(timePerMove).arg(numThreads);
    } else {
        qDebug().noquote() << QString("Running %1 WAC positions (fixed depth, threads: %2)...")
            .arg(total).arg(numThreads);
    }
    qDebug().noquote() << "-------------------------------------------";

    QElapsedTimer wallTimer;
    wallTimer.start();

    // Worker function — mỗi thread có Engine + MoveGen riêng
    // (MoveGen::instance là thread_local, mỗi thread cần init riêng)
    auto worker = [&]() {
        // Engine trên heap để tránh stack overflow
        // (Engine chứa ~110KB buffers + negamax recursion sâu có thể tràn 1MB stack)
        auto engine = std::make_unique<Engine>();
        MoveGen::init();  // Tạo MoveGen instance cho thread này

        while (true) {
            int i = nextTask.fetch_add(1);
            if (i >= total) break;

            const auto &e = entries[i];
            Board board;
            std::string fenStr = (e.fen + " 0 1").toStdString();
            parseFENString(fenStr, &board);

            QElapsedTimer timer;
            timer.start();

            Move bestMove;
            if (timePerMove > 0) {
                TimeInfo ti;
                ti.moveTimeMs = timePerMove;
                bestMove = engine->calc(board, ti);
            } else {
                bestMove = engine->calc(board);
            }

            qint64 elapsed = timer.elapsed();
            int depth = engine->getCompletedDepth();
            int score = engine->getBestScore();
            const auto &pv = engine->getPV();
            QString gotUci = moveToUci(bestMove);

            // Build PV string
            QString pvStr;
            for (int p = 0; p < pv.count; p++) {
                if (p > 0) pvStr += ' ';
                pvStr += moveToUci(pv.moves[p]);
            }

            // Tìm expected UCI (MoveGen::instance read-only, thread-safe)
            Move moveList[256];
            int moveCount = MoveGen::instance->genMoveList(board, moveList);
            QStringList expectedUciList;
            for (const auto &san : e.bmSan)
                for (int m = 0; m < moveCount; m++)
                    if (matchSan(san, moveList[m], board)) {
                        expectedUciList.append(moveToUci(moveList[m]));
                        break;
                    }

            bool ok = expectedUciList.contains(gotUci);

            // Ghi kết quả vào slot riêng (không cần lock)
            auto &r = results[i];
            r.gotUci = gotUci;
            r.pvStr = pvStr;
            r.expectedUciList = expectedUciList;
            r.depth = depth;
            r.score = score;
            r.pvCount = pv.count;
            r.elapsedMs = elapsed;
            r.ok = ok;

            int done = doneCount.fetch_add(1) + 1;
            QString status = ok ? "OK" : "FAIL";

            // Mate-in-N display (chính xác từ PV length)
            QString mateStr;
            if (score >= 32000 - 64) {
                int mateIn = (r.pvCount + 1) / 2;
                mateStr = QString("M%1").arg(mateIn);
            } else if (score <= -32000 + 64) {
                int mateIn = (r.pvCount + 1) / 2;
                mateStr = QString("-M%1").arg(mateIn);
            }

            // In progress (lock chỉ để tránh output lẫn lộn)
            {
                std::lock_guard<std::mutex> lock(printMtx);
                QString line = QString("[%1/%2] %3  expected=%4  got=%5  d=%6  %7  %8ms  %9")
                    .arg(done, 3).arg(total)
                    .arg(e.id, -10)
                    .arg(e.bmSan.join(","), -12)
                    .arg(gotUci, -7)
                    .arg(depth, 2)
                    .arg(mateStr, -4)
                    .arg(elapsed, 5)
                    .arg(status);
                if (!pvStr.isEmpty())
                    line += "  PV: " + pvStr;
                qDebug().noquote() << line;
            }
        }
    };

    // Spawn threads (stack 8MB — negamax deep recursion cần nhiều hơn default 1MB)
    std::vector<HANDLE> threadHandles;
    std::vector<std::function<void()>*> threadFuncs;
    for (int t = 0; t < numThreads; t++) {
        auto fn = new std::function<void()>(worker);
        HANDLE h = (HANDLE)_beginthreadex(
            nullptr,
            8 * 1024 * 1024,  // 8MB stack
            [](void* arg) -> unsigned {
                auto fn = static_cast<std::function<void()>*>(arg);
                (*fn)();
                return 0;
            },
            fn, 0, nullptr);
        threadHandles.push_back(h);
        threadFuncs.push_back(fn);
    }
    // Wait for all threads
    WaitForMultipleObjects((DWORD)threadHandles.size(), threadHandles.data(), TRUE, INFINITE);
    for (auto h : threadHandles) CloseHandle(h);
    for (auto fn : threadFuncs) delete fn;

    qint64 wallMs = wallTimer.elapsed();

    // Ghi CSV theo thứ tự gốc
    QFile csvFile(csvPath);
    csvFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream csv(&csvFile);
    csv << "id;fen;expected_san;expected_uci;got_uci;depth;score;time_ms;status;pv\n";

    int correct = 0;
    qint64 totalCpuMs = 0;
    for (int i = 0; i < total; i++) {
        const auto &e = entries[i];
        const auto &r = results[i];
        if (r.ok) correct++;
        totalCpuMs += r.elapsedMs;
        QString status = r.ok ? "OK" : "FAIL";

        csv << e.id << ";" << e.fen << ";" << e.bmSan.join(",") << ";"
            << r.expectedUciList.join(",") << ";" << r.gotUci << ";"
            << r.depth << ";" << r.score << ";" << r.elapsedMs << ";" << status
            << ";" << r.pvStr << "\n";
    }
    csvFile.close();

    qDebug().noquote() << "===========================================";
    qDebug().noquote() << QString("Result: %1/%2 correct (%3%)")
        .arg(correct).arg(total)
        .arg(correct * 100.0 / total, 0, 'f', 1);
    qDebug().noquote() << QString("Wall time: %1s  |  CPU time: %2s  |  Threads: %3")
        .arg(wallMs / 1000.0, 0, 'f', 1)
        .arg(totalCpuMs / 1000.0, 0, 'f', 1)
        .arg(numThreads);
    qDebug().noquote() << "CSV:" << csvPath;

    return 0;
}
