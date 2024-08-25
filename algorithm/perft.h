#pragma once
#include "board.h"

namespace kc {
class Perft
{
public:
    static void testPerft();

    static int genMoveRecur(Board &board, int depth);

    static std::string testFenPerft();
};
}
