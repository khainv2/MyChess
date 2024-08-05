#include "define.h"
#include "attack.h"
#include <QDebug>

using namespace kchess;

std::string kchess::getMoveDescription(Move move)
{
    auto src = getSourceFromMove(move);
    auto dst = getDestinationFromMove(move);
    static std::string fileTxt[8] = { "A", "B", "C", "D", "E", "F", "G", "H" };
    static std::string rankTxt[8] = { "1", "2", "3", "4", "5", "6", "7", "8" };

    int srcFileIdx = src % 8;
    int srcRankIdx = src / 8;
    int dstFileIdx = dst % 8;
    int dstRankIdx = dst / 8;
    return "From '" + fileTxt[srcFileIdx] + rankTxt[srcRankIdx] + "'"
            + " to '" + fileTxt[dstFileIdx] + rankTxt[dstRankIdx] + "'";
}
