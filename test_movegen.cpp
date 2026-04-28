/**
 * Perft-based move generation correctness test.
 * Build: MyChessTest.pro
 * Usage: ./MyChessTest
 *
 * Chạy perft trên nhiều FEN position đã biết kết quả chính xác.
 * Dùng để verify move generation trước và sau refactor.
 * Exit code 0 = all pass, 1 = có test fail.
 */
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>

#include "algorithm/attack.h"
#include "algorithm/board.h"
#include "algorithm/movegen.h"
#include "algorithm/evaluation.h"
#include "algorithm/util.h"

using namespace kc;

// ── Perft recursive ─────────────────────────────────────────────────
static Move perftMoves[20][256];

static u64 perft(Board &board, int depth) {
    if (depth == 0) return 1;
    if (depth == 1) {
        return MoveGen::instance->countMoveList(board);
    }
    Move *moveList = perftMoves[depth];
    int count = MoveGen::instance->genMoveList(board, moveList);
    u64 total = 0;
    BoardState state;
    for (int i = 0; i < count; i++) {
        board.doMove(moveList[i], state);
        total += perft(board, depth - 1);
        board.undoMove(moveList[i]);
    }
    return total;
}

// ── Test case definition ────────────────────────────────────────────
struct PerftTest {
    const char *name;
    const char *fen;
    int depth;
    u64 expected;
};

// Bộ test chuẩn: https://www.chessprogramming.org/Perft_Results
static const PerftTest tests[] = {
    // ═══════════════════════════════════════════════════════════════
    // Position 1: Starting position
    // ═══════════════════════════════════════════════════════════════
    {"startpos d1",
     "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     1, 20},
    {"startpos d2",
     "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     2, 400},
    {"startpos d3",
     "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     3, 8902},
    {"startpos d4",
     "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     4, 197281},
    {"startpos d5",
     "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     5, 4865609},
    {"startpos d6",
     "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     6, 119060324},

    // ═══════════════════════════════════════════════════════════════
    // Position 2: "Kiwipete" — castling, en passant, promotions, pins
    // ═══════════════════════════════════════════════════════════════
    {"kiwipete d1",
     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     1, 48},
    {"kiwipete d2",
     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     2, 2039},
    {"kiwipete d3",
     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     3, 97862},
    {"kiwipete d4",
     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     4, 4085603},
    {"kiwipete d5",
     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     5, 193690690},

    // ═══════════════════════════════════════════════════════════════
    // Position 3: En passant, pawn promotion edge cases
    // ═══════════════════════════════════════════════════════════════
    {"pos3 d1",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     1, 14},
    {"pos3 d2",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     2, 191},
    {"pos3 d3",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     3, 2812},
    {"pos3 d4",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     4, 43238},
    {"pos3 d5",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     5, 674624},
    {"pos3 d6",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     6, 11030083},
    {"pos3 d7",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     7, 178633661},

    // ═══════════════════════════════════════════════════════════════
    // Position 4: Mirror position — nhiều pin, check, castling
    // ═══════════════════════════════════════════════════════════════
    {"pos4 d1",
     "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     1, 6},
    {"pos4 d2",
     "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     2, 264},
    {"pos4 d3",
     "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     3, 9467},
    {"pos4 d4",
     "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     4, 422333},
    {"pos4 d5",
     "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     5, 15833292},

    // Position 4 mirrored (Black to move)
    {"pos4m d1",
     "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
     1, 6},
    {"pos4m d2",
     "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
     2, 264},
    {"pos4m d3",
     "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
     3, 9467},
    {"pos4m d4",
     "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
     4, 422333},
    {"pos4m d5",
     "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
     5, 15833292},

    // ═══════════════════════════════════════════════════════════════
    // Position 5: Complex middlegame — double pins, discovered checks
    // ═══════════════════════════════════════════════════════════════
    {"pos5 d1",
     "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     1, 44},
    {"pos5 d2",
     "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     2, 1486},
    {"pos5 d3",
     "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     3, 62379},
    {"pos5 d4",
     "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     4, 2103487},
    {"pos5 d5",
     "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     5, 89941194},

    // ═══════════════════════════════════════════════════════════════
    // Position 6: Symmetric middlegame
    // ═══════════════════════════════════════════════════════════════
    {"pos6 d1",
     "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     1, 46},
    {"pos6 d2",
     "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     2, 2079},
    {"pos6 d3",
     "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     3, 89890},
    {"pos6 d4",
     "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     4, 3894594},
    {"pos6 d5",
     "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     5, 164075551},

    // ═══════════════════════════════════════════════════════════════
    // Edge cases: en passant pin, promotion+check, tricky positions
    // ═══════════════════════════════════════════════════════════════

    // EP horizontal pin: Ka4, Pd4(white), Pe4(black), Rh4(white), Ke1(white)
    // EP bất hợp lệ vì bắt EP sẽ mở đường xe tấn công vua
    {"ep-hpin d1",
     "8/8/8/8/k2Pp2R/8/8/4K3 b - d3 0 1",
     1, 6},
    {"ep-hpin d2",
     "8/8/8/8/k2Pp2R/8/8/4K3 b - d3 0 1",
     2, 94},
    {"ep-hpin d3",
     "8/8/8/8/k2Pp2R/8/8/4K3 b - d3 0 1",
     3, 640},
    {"ep-hpin d4",
     "8/8/8/8/k2Pp2R/8/8/4K3 b - d3 0 1",
     4, 10826},

    // EP capture: Kc5 cạnh Pd4(white)+Pe4(black), Ke1(white)
    {"ep-near-king d1",
     "8/8/8/2k5/3Pp3/8/8/4K3 b - d3 0 1",
     1, 8},
    {"ep-near-king d2",
     "8/8/8/2k5/3Pp3/8/8/4K3 b - d3 0 1",
     2, 46},
    {"ep-near-king d3",
     "8/8/8/2k5/3Pp3/8/8/4K3 b - d3 0 1",
     3, 344},
    {"ep-near-king d4",
     "8/8/8/2k5/3Pp3/8/8/4K3 b - d3 0 1",
     4, 2202},

    // Promotion with all 4 piece types
    {"promo d1",
     "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
     1, 24},
    {"promo d2",
     "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
     2, 496},
    {"promo d3",
     "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
     3, 9483},
    {"promo d4",
     "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
     4, 182838},

    // Castling: cả 2 bên đều có quyền nhập thành, bàn trống
    {"castle-open d1",
     "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
     1, 26},
    {"castle-open d2",
     "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
     2, 568},
    {"castle-open d3",
     "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
     3, 13744},
    {"castle-open d4",
     "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
     4, 314346},
    {"castle-open d5",
     "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
     5, 7594526},
};

static const int numTests = sizeof(tests) / sizeof(tests[0]);

// ── Main ────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    attack::init();
    MoveGen::init();
    eval::init();

    bool quickMode = false;
    for (int i = 1; i < argc; i++) {
        if (QString(argv[i]) == "--quick") quickMode = true;
    }

    int passed = 0, failed = 0, skipped = 0;
    qint64 totalMs = 0;
    QElapsedTimer totalTimer;
    totalTimer.start();

    qDebug().noquote() << "═══════════════════════════════════════════════";
    qDebug().noquote() << "  Move Generation Perft Test Suite";
    qDebug().noquote() << "═══════════════════════════════════════════════";
    qDebug().noquote() << "";

    for (int t = 0; t < numTests; t++) {
        const auto &test = tests[t];

        // --quick: bỏ qua test depth >= 6 (chạy lâu)
        if (quickMode && test.depth >= 6) {
            skipped++;
            continue;
        }

        Board board;
        std::string fen = test.fen;
        parseFENString(fen, &board);

        QElapsedTimer timer;
        timer.start();
        u64 result = perft(board, test.depth);
        qint64 elapsed = timer.elapsed();
        totalMs += elapsed;

        bool ok = (result == test.expected);
        if (ok) {
            passed++;
            qDebug().noquote() << QString("  PASS  %1  depth=%2  nodes=%3  %4ms")
                .arg(test.name, -20).arg(test.depth).arg(result).arg(elapsed);
        } else {
            failed++;
            qDebug().noquote() << QString("  FAIL  %1  depth=%2  expected=%3  got=%4")
                .arg(test.name, -20).arg(test.depth).arg(test.expected).arg(result);
            qDebug().noquote() << QString("        FEN: %1").arg(test.fen);
        }
    }

    qDebug().noquote() << "";
    qDebug().noquote() << "═══════════════════════════════════════════════";
    qDebug().noquote() << QString("  Result: %1 passed, %2 failed, %3 skipped")
        .arg(passed).arg(failed).arg(skipped);
    qDebug().noquote() << QString("  Total time: %1ms").arg(totalTimer.elapsed());
    qDebug().noquote() << "═══════════════════════════════════════════════";

    return failed > 0 ? 1 : 0;
}
