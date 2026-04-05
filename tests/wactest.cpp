/**
 * WAC Test Runner for MyChess engine.
 * Parses wac.epd, runs Engine::calc() on each position, outputs CSV.
 *
 * Build: included in MyChess.pro, invoked via MainWindow button or command line.
 * This file provides WacTestRunner class that can be called from the Qt app.
 */
#include "wactest.h"

#include "../algorithm/attack.h"
#include "../algorithm/evaluation.h"
#include "../algorithm/movegen.h"
#include "../algorithm/engine.h"
#include "../algorithm/util.h"

#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QDebug>
#include <QRegularExpression>

static QString squareToUci(int sq) {
    char file = 'a' + (sq % 8);
    char rank = '1' + (sq / 8);
    return QString("%1%2").arg(file).arg(rank);
}

static QString moveToUci(kc::Move move) {
    QString uci = squareToUci(move.src()) + squareToUci(move.dst());
    if (move.is<kc::Move::Promotion>() || move.is<kc::Move::CapturePromotion>()) {
        kc::PieceType pt = move.getPromotionPieceType();
        const char promo[] = {0, 0, 'n', 'b', 'r', 'q', 0};
        if (pt >= kc::Knight && pt <= kc::Queen)
            uci += promo[pt];
    }
    return uci;
}

struct EpdEntry {
    QString id;
    QString fen;        // 4-field FEN from EPD
    QStringList bmSan;  // best moves in SAN (not used for comparison, just display)
};

static QVector<EpdEntry> parseEpd(const QString &path) {
    QVector<EpdEntry> entries;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return entries;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(' ');
        if (parts.size() < 5) continue;

        EpdEntry e;
        e.fen = parts.mid(0, 4).join(' ');

        QString ops = parts.mid(4).join(' ');

        // Extract bm
        QRegularExpression bmRe("bm\\s+([^;]+);");
        auto bmMatch = bmRe.match(ops);
        if (bmMatch.hasMatch()) {
            for (auto &m : bmMatch.captured(1).split(' ', QString::SkipEmptyParts))
                e.bmSan.append(m.trimmed());
        }

        // Extract id
        QRegularExpression idRe("id\\s+\"([^\"]+)\"");
        auto idMatch = idRe.match(ops);
        e.id = idMatch.hasMatch() ? idMatch.captured(1) : "?";

        entries.append(e);
    }
    return entries;
}

WacTestRunner::WacTestRunner(QObject *parent) : QObject(parent) {}

void WacTestRunner::run(const QString &epdPath, const QString &csvPath) {
    auto entries = parseEpd(epdPath);
    if (entries.isEmpty()) {
        emit finished(0, 0, "Failed to load EPD file");
        return;
    }

    QFile csvFile(csvPath);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit finished(0, 0, "Failed to open CSV for writing");
        return;
    }

    QTextStream csv(&csvFile);
    csv << "id;fen;expected_san;expected_uci;got_uci;score;status\n";

    kc::Engine engine;
    int total = entries.size();
    int correct = 0;

    QElapsedTimer totalTimer;
    totalTimer.start();

    for (int i = 0; i < total; i++) {
        const auto &e = entries[i];

        kc::Board board;
        std::string fenStr = (e.fen + " 0 1").toStdString();
        parseFENString(fenStr, &board);

        QElapsedTimer timer;
        timer.start();

        kc::Move bestMove = engine.calc(board);
        qint64 elapsed = timer.elapsed();

        QString gotUci = moveToUci(bestMove);

        // Generate all legal moves to find expected UCI moves
        kc::Move moveList[256];
        int moveCount = kc::MoveGen::instance->genMoveList(board, moveList);

        QStringList expectedUciList;
        for (const auto &san : e.bmSan) {
            // Match SAN against legal moves by trying each and comparing description
            // Simple heuristic: match by destination and piece type
            for (int m = 0; m < moveCount; m++) {
                QString desc = QString::fromStdString(moveList[m].getDescription());
                QString uci = moveToUci(moveList[m]);
                // Try matching the SAN pattern
                if (matchSan(san, moveList[m], board)) {
                    expectedUciList.append(uci);
                    break;
                }
            }
        }

        bool isCorrect = expectedUciList.contains(gotUci);
        if (isCorrect) correct++;
        QString status = isCorrect ? "OK" : "FAIL";

        csv << e.id << ";"
            << e.fen << ";"
            << e.bmSan.join(",") << ";"
            << expectedUciList.join(",") << ";"
            << gotUci << ";"
            << "depth 4" << ";"
            << status << "\n";

        qDebug().noquote() << QString("[%1/%2] %3  expected=%4  got=%5  %6ms  %7")
            .arg(i + 1, 3).arg(total)
            .arg(e.id, -10)
            .arg(e.bmSan.join(","), -12)
            .arg(gotUci, -7)
            .arg(elapsed, 4)
            .arg(status);

        emit progress(i + 1, total, e.id, status, gotUci, elapsed);
    }

    qint64 totalMs = totalTimer.elapsed();
    csvFile.close();

    QString summary = QString("%1/%2 correct (%3%) in %4s")
        .arg(correct).arg(total)
        .arg(correct * 100.0 / total, 0, 'f', 1)
        .arg(totalMs / 1000.0, 0, 'f', 1);

    qDebug().noquote() << "=========================================";
    qDebug().noquote() << "Result:" << summary;
    qDebug().noquote() << "CSV:" << csvPath;

    emit finished(correct, total, summary);
}

bool WacTestRunner::matchSan(const QString &san, kc::Move move, const kc::Board &board) {
    // Convert move to a comparable format and match against SAN
    int src = move.src();
    int dst = move.dst();
    int srcFile = src % 8;
    int srcRank = src / 8;
    int dstFile = dst % 8;
    int dstRank = dst / 8;

    kc::Piece piece = board.pieces[src];
    kc::PieceType pt = kc::pieceToType(piece);

    QString s = san;
    // Remove check/mate symbols
    s.remove('+');
    s.remove('#');

    // Castling
    if (s == "O-O" || s == "O-O-O") {
        if (s == "O-O" && move.is<kc::Move::KingCastling>()) return true;
        if (s == "O-O-O" && move.is<kc::Move::QueenCastling>()) return true;
        return false;
    }

    // Pawn moves: e4, exd5, e8=Q
    // Piece moves: Nf3, Bxe5, Rad1, R1d4, Qxf7+

    // Determine expected piece type from SAN
    kc::PieceType expectedPt = kc::Pawn;
    int sanIdx = 0;
    if (!s.isEmpty() && s[0].isUpper() && s[0] != 'O') {
        QChar ch = s[0];
        if (ch == 'N') expectedPt = kc::Knight;
        else if (ch == 'B') expectedPt = kc::Bishop;
        else if (ch == 'R') expectedPt = kc::Rook;
        else if (ch == 'Q') expectedPt = kc::Queen;
        else if (ch == 'K') expectedPt = kc::King;
        sanIdx = 1;
    }

    if (pt != expectedPt) return false;

    // Handle promotion: find =X at end
    if (s.contains('=')) {
        if (!move.is<kc::Move::Promotion>() && !move.is<kc::Move::CapturePromotion>())
            return false;
        int eqIdx = s.indexOf('=');
        QChar promoCh = s[eqIdx + 1];
        kc::PieceType promoPt = kc::Pawn;
        if (promoCh == 'N') promoPt = kc::Knight;
        else if (promoCh == 'B') promoPt = kc::Bishop;
        else if (promoCh == 'R') promoPt = kc::Rook;
        else if (promoCh == 'Q') promoPt = kc::Queen;
        if (move.getPromotionPieceType() != promoPt) return false;
        s = s.left(eqIdx);
    }

    // Parse remaining: optional disambiguation + optional 'x' + destination
    QString remainder = s.mid(sanIdx);
    remainder.remove('x');

    // Last two chars should be destination square
    if (remainder.size() < 2) return false;
    QChar dstFileChar = remainder[remainder.size() - 2];
    QChar dstRankChar = remainder[remainder.size() - 1];

    if (dstFileChar < 'a' || dstFileChar > 'h') return false;
    if (dstRankChar < '1' || dstRankChar > '8') return false;

    int expectedDstFile = dstFileChar.toLatin1() - 'a';
    int expectedDstRank = dstRankChar.toLatin1() - '1';

    if (dstFile != expectedDstFile || dstRank != expectedDstRank) return false;

    // Check disambiguation (if any)
    QString disambig = remainder.left(remainder.size() - 2);
    if (!disambig.isEmpty()) {
        for (QChar c : disambig) {
            if (c >= 'a' && c <= 'h') {
                if (srcFile != (c.toLatin1() - 'a')) return false;
            } else if (c >= '1' && c <= '8') {
                if (srcRank != (c.toLatin1() - '1')) return false;
            }
        }
    }

    return true;
}
