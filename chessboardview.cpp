#include "chessboardview.h"
#include <QPainter>
#include <QDebug>
#include <util.h>
#include <QMouseEvent>

using namespace kchess;
ChessBoardView::ChessBoardView(QWidget *parent) : QWidget(parent)
{
    _pixmaps[King * 2 + White] = QPixmap(":/w_king.png");
    _pixmaps[King * 2 + Black] = QPixmap(":/b_king.png");
    _pixmaps[Queen * 2 + White] = QPixmap(":/w_queen.png");
    _pixmaps[Queen * 2 + Black] = QPixmap(":/b_queen.png");
    _pixmaps[Rook * 2 + White] = QPixmap(":/w_rook.png");
    _pixmaps[Rook * 2 + Black] = QPixmap(":/b_rook.png");
    _pixmaps[Bishop * 2 + White] = QPixmap(":/w_bishop.png");
    _pixmaps[Bishop * 2 + Black] = QPixmap(":/b_bishop.png");
    _pixmaps[Knight * 2 + White] = QPixmap(":/w_knight.png");
    _pixmaps[Knight * 2 + Black] = QPixmap(":/b_knight.png");
    _pixmaps[Pawn * 2 + White] = QPixmap(":/w_pawn.png");
    _pixmaps[Pawn * 2 + Black] = QPixmap(":/b_pawn.png");


//    parseFENString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &board);
//    parseFENString("4R3/R7/8/K3R3/R7/1R6/8/8 w KQkq - 0 1", &board);
//    parseFENString("4B3/B7/8/K3B3/B7/1B6/B7/7B w KQkq - 0 1", &board);
//    parseFENString("4N3/N7/8/K3N3/B7/1B6/B7/7B w KQkq - 0 1", &board);
    parseFENString("rnbqkbnr/pppppppp/8/3p1P2/4P3/1P6/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &board);
}

void ChessBoardView::paintEvent(QPaintEvent *event)
{
    // Draw board
    int w = width(), h = height();
    int side = qMin(w, h);
    QRect boundary = w < h ? QRect(0, ((h - w) / 2), side, side) : QRect((w - h) / 2, 0, side, side);
    QPainter painter(this);

    canvasRect = boundary - QMargins(_borderSize, _borderSize, _borderSize, _borderSize);
    QPen pen;
    pen.setWidth(2);
    painter.setPen(pen);
    painter.drawRect(canvasRect);

    int ss = canvasRect.width() / 8;
    for (int i = 0; i < 8; i++){
        for (int j = 0; j < 8; j++){
            if ((i + j) % 2 == 0){
                QRect square(i * canvasRect.width() / 8 + canvasRect.left(), j * canvasRect.width() / 8 + canvasRect.top(), ss, ss);
                painter.fillRect(square, Qt::black);
            }
        }
    }

    // Draw all piece
    QMargins pieceMargin(4, 4, 4, 4);
    for (int i = 0; i < 8; i++){
        for (int j = 0; j < 8; j++){
            QRect square(i * canvasRect.width() / 8 + canvasRect.left(),
                         j * canvasRect.width() / 8 + canvasRect.top(),
                         ss, ss);
            int index = (7 - j) * 8 + i;
            for (int piece = 0; piece < Piece_NB; piece++){
                for (int color = 0; color < Color_NB; color++){
                    auto bitboard = board.getBitBoard(Piece(piece), Color(color));
                    const auto &pixmap = _pixmaps[piece * 2 + color];
                    if (bitboard & squareToBB(index)){
                        painter.drawPixmap(square - pieceMargin, pixmap);
                    }
                }
            }

            if (mouseSelection == squareToBB(index)){
                painter.save();
                QRect square(i * canvasRect.width() / 8 + canvasRect.left(),
                             j * canvasRect.width() / 8 + canvasRect.top(),
                             ss, ss);
                painter.setBrush(QBrush(QColor("#88aa0000")));
                painter.drawRect(square);
                painter.restore();
            }

            if (mobility & squareToBB(index)){
                painter.save();
                QRect square(i * canvasRect.width() / 8 + canvasRect.left(),
                             j * canvasRect.width() / 8 + canvasRect.top(),
                             ss, ss);
                painter.setBrush(QBrush(QColor("#8811ffaa")));
                painter.drawRect(square);
                painter.restore();
            }
        }
    }
}

void ChessBoardView::mousePressEvent(QMouseEvent *event)
{
    auto pos = event->pos();
    if (event->button() == Qt::LeftButton){
        if (canvasRect.contains(pos)){
            int ss = canvasRect.width() / 8;
            int i = (pos.x() - canvasRect.left()) / ss;
            int j = (pos.y() - canvasRect.top()) / ss;
            int index = (7 - j) * 8 + i;
            auto bb = squareToBB(index);
            if (mobility & bb){
                // Clear all des
                auto bblist = reinterpret_cast<u64 *>(&board);
                for (int i = 0; i < 8; i++){
                    auto v = bblist[i] & mouseSelection;
                    if (v) bblist[i] |= bb;
                    else bblist[i] &= (~bb);
                    bblist[i] &= (~mouseSelection);
                }
                mouseSelection = 0;
                mobility = 0;
            } else {
                mouseSelection = bb;
                mobility = getMobility(board, Square(index));
            }
            update();
        }
    } else {
        mouseSelection = 0;
        mobility = 0;
        update();
    }

//    auto b = getMobility(board);
}
