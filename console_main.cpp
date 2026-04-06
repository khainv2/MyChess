/**
 * Console-only WAC test runner (no GUI).
 * Build with MyChessConsole.pro
 * Usage: ./MyChessConsole <epd_path> <csv_output_path> [--limit N]
 */
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QRegularExpression>

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

// ── main ─────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Defaults
    QString epdPath = "tests/wac.epd";
    QString csvPath = "wac_console_results.csv";
    int limit = 0;  // 0 = all

    QStringList args = app.arguments();
    for (int i = 1; i < args.size(); i++) {
        if (args[i] == "--limit" && i + 1 < args.size())
            limit = args[++i].toInt();
        else if (args[i] == "--epd" && i + 1 < args.size())
            epdPath = args[++i];
        else if (args[i] == "--csv" && i + 1 < args.size())
            csvPath = args[++i];
    }

    // Init engine
    attack::init();
    MoveGen::init();
    eval::init();
    kc::zobrist::init();

    auto entries = parseEpd(epdPath);
    if (entries.isEmpty()) {
        qCritical() << "Cannot load EPD:" << epdPath;
        return 1;
    }

    int total = (limit > 0 && limit < entries.size()) ? limit : entries.size();

    QFile csvFile(csvPath);
    csvFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream csv(&csvFile);
    csv << "id;fen;expected_san;expected_uci;got_uci;time_ms;status\n";

    Engine engine;
    int correct = 0;
    qint64 totalMs = 0;

    qDebug().noquote() << QString("Running %1 WAC positions...").arg(total);
    qDebug().noquote() << "-------------------------------------------";

    for (int i = 0; i < total; i++) {
        const auto &e = entries[i];
        Board board;
        std::string fenStr = (e.fen + " 0 1").toStdString();
        parseFENString(fenStr, &board);

        QElapsedTimer timer;
        timer.start();
        Move bestMove = engine.calc(board);
        qint64 elapsed = timer.elapsed();
        totalMs += elapsed;

        QString gotUci = moveToUci(bestMove);

        // Find expected UCI
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
        if (ok) correct++;
        QString status = ok ? "OK" : "FAIL";

        csv << e.id << ";" << e.fen << ";" << e.bmSan.join(",") << ";"
            << expectedUciList.join(",") << ";" << gotUci << ";"
            << elapsed << ";" << status << "\n";

        qDebug().noquote() << QString("[%1/%2] %3  expected=%4  got=%5  %6ms  %7")
            .arg(i + 1, 3).arg(total)
            .arg(e.id, -10)
            .arg(e.bmSan.join(","), -12)
            .arg(gotUci, -7)
            .arg(elapsed, 5)
            .arg(status);
    }

    csvFile.close();

    qDebug().noquote() << "===========================================";
    qDebug().noquote() << QString("Result: %1/%2 correct (%3%) in %4s")
        .arg(correct).arg(total)
        .arg(correct * 100.0 / total, 0, 'f', 1)
        .arg(totalMs / 1000.0, 0, 'f', 1);
    qDebug().noquote() << "CSV:" << csvPath;

    return 0;
}
