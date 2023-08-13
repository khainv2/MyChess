#pragma once
#include <algorithm/define.h>
#include <QString>

bool parseFENString(const QString &fen, kchess::ChessBoard *result);
