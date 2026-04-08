/**
 * UCI Protocol interface for MyChess engine.
 * Build with MyChessUCI.pro
 */
#include <QCoreApplication>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
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

// ── Move conversion ─────────────────────────────────────────────────

static std::string sqToUci(int sq) {
    return std::string(1, 'a' + sq % 8) + std::string(1, '1' + sq / 8);
}

static std::string moveToUci(Move m) {
    std::string uci = sqToUci(m.src()) + sqToUci(m.dst());
    if (m.is<Move::Promotion>() || m.is<Move::CapturePromotion>()) {
        const char promo[] = {0, 0, 'n', 'b', 'r', 'q', 0};
        PieceType pt = m.getPromotionPieceType();
        if (pt >= Knight && pt <= Queen) uci += promo[pt];
    }
    return uci;
}

static std::string pvToUci(const PVLine &pv) {
    std::string s;
    for (int i = 0; i < pv.count; i++) {
        if (i > 0) s += ' ';
        s += moveToUci(pv.moves[i]);
    }
    return s;
}

static Move parseUciMove(const Board &board, const std::string &uci) {
    if (uci.size() < 4) return Move();

    int srcFile = uci[0] - 'a', srcRank = uci[1] - '1';
    int dstFile = uci[2] - 'a', dstRank = uci[3] - '1';
    int src = srcRank * 8 + srcFile;
    int dst = dstRank * 8 + dstFile;

    PieceType promoPt = PieceTypeNone;
    if (uci.size() >= 5) {
        switch (uci[4]) {
            case 'n': promoPt = Knight; break;
            case 'b': promoPt = Bishop; break;
            case 'r': promoPt = Rook;   break;
            case 'q': promoPt = Queen;  break;
        }
    }

    // Generate all legal moves, find the one matching
    Move moveList[256];
    int count = MoveGen::instance->genMoveList(board, moveList);
    for (int i = 0; i < count; i++) {
        Move m = moveList[i];
        if (m.src() != src || m.dst() != dst) continue;
        if (promoPt != PieceTypeNone) {
            if ((m.is<Move::Promotion>() || m.is<Move::CapturePromotion>())
                && m.getPromotionPieceType() == promoPt)
                return m;
        } else {
            if (!m.is<Move::Promotion>() && !m.is<Move::CapturePromotion>())
                return m;
        }
    }
    return Move(); // not found
}

// ── UCI score string ────────────────────────────────────────────────

static std::string scoreToUci(int score) {
    if (score >= ScoreMate - 64) {
        int mateIn = (ScoreMate - score + 1) / 2;
        return "mate " + std::to_string(mateIn);
    }
    if (score <= -ScoreMate + 64) {
        int mateIn = -(ScoreMate + score + 1) / 2;
        return "mate " + std::to_string(mateIn);
    }
    return "cp " + std::to_string(score);
}

// ── Global state ────────────────────────────────────────────────────

static Board currentBoard;
static std::vector<BoardState> stateHistory;
static BoardState rootState; // BoardState for initial position
static std::unique_ptr<Engine> engine;
static HANDLE searchThread = nullptr;
static std::atomic<bool> searching{false};
static std::mutex outputMtx;

static void syncOutput(const std::string &line) {
    std::lock_guard<std::mutex> lock(outputMtx);
    std::cout << line << std::endl;
}

// ── Search thread ───────────────────────────────────────────────────

struct SearchArgs {
    Board board;
    TimeInfo ti;
};

static unsigned __stdcall searchWorker(void *arg) {
    auto *args = static_cast<SearchArgs *>(arg);

    MoveGen::init(); // thread_local init

    Move best = engine->calc(args->board, args->ti);

    syncOutput("bestmove " + moveToUci(best));
    searching = false;
    delete args;
    return 0;
}

static void startSearch(const Board &board, const TimeInfo &ti) {
    if (searching) return;
    searching = true;

    auto *args = new SearchArgs{board, ti};

    searchThread = (HANDLE)_beginthreadex(
        nullptr,
        8 * 1024 * 1024, // 8MB stack
        searchWorker,
        args, 0, nullptr);
}

static void stopSearch() {
    if (!searching) return;
    engine->abort();
    if (searchThread) {
        WaitForSingleObject(searchThread, INFINITE);
        CloseHandle(searchThread);
        searchThread = nullptr;
    }
}

// ── Command handlers ────────────────────────────────────────────────

static void handleUci() {
    syncOutput("id name MyChess");
    syncOutput("id author khai nguyen");
    syncOutput("uciok");
}

static void handleIsReady() {
    syncOutput("readyok");
}

static void handlePosition(std::istringstream &is) {
    std::string token;
    is >> token;

    // Reset board
    currentBoard = Board();
    stateHistory.clear();

    if (token == "startpos") {
        std::string startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        parseFENString(startFen, &currentBoard);
        // Consume "moves" token if present
        is >> token;
    } else if (token == "fen") {
        std::string fen;
        // FEN has 6 parts
        for (int i = 0; i < 6 && is >> token; i++) {
            if (i > 0) fen += ' ';
            fen += token;
        }
        parseFENString(fen, &currentBoard);
        is >> token; // might be "moves" or empty
    }

    currentBoard.initHash();

    // Apply moves
    if (token == "moves") {
        while (is >> token) {
            Move m = parseUciMove(currentBoard, token);
            if (m.raw() == 0) break;
            stateHistory.emplace_back();
            currentBoard.doMove(m, stateHistory.back());
        }
    }
}

static void handleGo(std::istringstream &is) {
    TimeInfo ti;
    bool infinite = false;

    std::string token;
    while (is >> token) {
        if (token == "depth") { is >> ti.depth; }
        else if (token == "movetime") { is >> ti.moveTimeMs; }
        else if (token == "wtime" && currentBoard.side == White) { is >> ti.remainingMs; }
        else if (token == "btime" && currentBoard.side == Black) { is >> ti.remainingMs; }
        else if (token == "wtime" && currentBoard.side == Black) { int dummy; is >> dummy; }
        else if (token == "btime" && currentBoard.side == White) { int dummy; is >> dummy; }
        else if (token == "winc" && currentBoard.side == White) { is >> ti.incrementMs; }
        else if (token == "binc" && currentBoard.side == Black) { is >> ti.incrementMs; }
        else if (token == "winc" && currentBoard.side == Black) { int dummy; is >> dummy; }
        else if (token == "binc" && currentBoard.side == White) { int dummy; is >> dummy; }
        else if (token == "movestogo") { is >> ti.movesToGo; }
        else if (token == "infinite") { infinite = true; }
    }

    // infinite = no time limit, rely on "stop"
    if (infinite) {
        ti.depth = 0;
        ti.moveTimeMs = 0;
        ti.remainingMs = 0;
    }

    startSearch(currentBoard, ti);
}

static void handleStop() {
    stopSearch();
}

// ── Info callback ───────────────────────────────────────────────────

static void infoCallback(int depth, int score, int timeMs, u64 nodes, const PVLine &pv) {
    u64 nps = timeMs > 0 ? nodes * 1000 / timeMs : 0;
    std::string line = "info depth " + std::to_string(depth)
        + " score " + scoreToUci(score)
        + " time " + std::to_string(timeMs)
        + " nodes " + std::to_string(nodes)
        + " nps " + std::to_string(nps);
    if (pv.count > 0)
        line += " pv " + pvToUci(pv);
    syncOutput(line);
}

// ── Main ────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Flush stdout after every output
    std::cout.setf(std::ios::unitbuf);

    // Init engine globals
    attack::init();
    MoveGen::init();
    eval::init();
    kc::zobrist::init();

    engine = std::make_unique<Engine>();
    engine->setInfoCallback(infoCallback);

    // UCI input loop
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream is(line);
        std::string cmd;
        is >> cmd;
        if (cmd.empty()) continue;

        if (cmd == "uci")          handleUci();
        else if (cmd == "isready") handleIsReady();
        else if (cmd == "position") handlePosition(is);
        else if (cmd == "go")      handleGo(is);
        else if (cmd == "stop")    handleStop();
        else if (cmd == "ucinewgame") { /* engine clears TT in calc() */ }
        else if (cmd == "quit")    { stopSearch(); break; }
    }

    return 0;
}
