#include "test.h"
#include <algorithm/chessboard.h>
#include <util.h>
#include <QString>

using namespace kchess;

void test_parseFENString() {
    ChessBoard chessBoard;

    // Test case 1: Valid FEN string
    QString fen1 = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    assert(parseFENString(fen1, &chessBoard) == true);
    // Add assertions to check the values of the chessBoard object

    // Test case 2: Invalid FEN string with missing fields
    QString fen2 = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0";
    assert(parseFENString(fen2, &chessBoard) == false);

    // Test case 3: Invalid FEN string with incorrect piece data
    QString fen3 = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 2";
    assert(parseFENString(fen3, &chessBoard) == false);

    // Test case 4: Invalid FEN string with incorrect active color
    QString fen4 = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1";
    assert(parseFENString(fen4, &chessBoard) == false);

    // Test case 5: Invalid FEN string with incorrect castling availability
    QString fen5 = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w ABCD - 0 1";
    assert(parseFENString(fen5, &chessBoard) == false);

    // Test case 6: Invalid FEN string with incorrect en passant target square
    QString fen6 = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq A3 0 1";
    assert(parseFENString(fen6, &chessBoard) == false);

    // Test case 7: Invalid FEN string with incorrect half move clock
    QString fen7 = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - ABC 1";
    assert(parseFENString(fen7, &chessBoard) == false);

    // Test case 8: Invalid FEN string with incorrect full move number
    QString fen8 = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 ABC";
    assert(parseFENString(fen8, &chessBoard) == false);
}


void testAll()
{
    test_parseFENString();
}
