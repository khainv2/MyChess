#pragma once
#include <algorithm/chessboard.h>
#include <QString>

bool parseFENString(const QString &fen, kchess::ChessBoard *result);

QString toFenString(const kchess::ChessBoard &chessBoard);
