#include "chessboardview.h"
#include "algorithm/evaluation.h"
#include <QPainter>
#include <QDebug>
#include <util.h>
#include <QMouseEvent>
#include <algorithm/evaluation.h>

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
    


    parseFENString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &_board);
//    parseFENString("4R3/R7/8/K3R3/R7/1R6/8/8 w KQkq - 0 1", &board);
//    parseFENString("4B3/B7/8/K3B3/B7/1B6/B7/7B w KQkq - 0 1", &board);
//    parseFENString("4N3/N7/8/K3N3/B7/1B6/B7/7B w KQkq - 0 1", &board);
//    parseFENString("rnbqkbnr/pppppppp/8/3p1P2/4P3/1P6/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &_board);
}

void ChessBoardView::paintEvent(QPaintEvent *event)
{
    // Draw board
    int w = width(), h = height();
    int side = qMin(w, h);
    QRect boundary = w < h ? QRect(0, 0, side, side) : QRect(0, 0, side, side);
    QPainter painter(this);

    _canvasRect = boundary - QMargins(_borderSize, _borderSize, _borderSize, _borderSize);
    QPen pen;
    pen.setWidth(2);
    painter.setPen(pen);
    painter.drawRect(_canvasRect);

    int ss = _canvasRect.width() / 8;
    for (int i = 0; i < 8; i++){
        for (int j = 0; j < 8; j++){
            if ((i + j) % 2 == 0){
                QRect square(i * _canvasRect.width() / 8 + _canvasRect.left(), j * _canvasRect.width() / 8 + _canvasRect.top(), ss, ss);
                painter.fillRect(square, QColor(0x29180d));
            }
        }
    }

    // Draw all piece
    QMargins pieceMargin(4, 4, 4, 4);
    for (int i = 0; i < 8; i++){
        for (int j = 0; j < 8; j++){
            QRect square(i * _canvasRect.width() / 8 + _canvasRect.left(),
                         j * _canvasRect.width() / 8 + _canvasRect.top(),
                         ss, ss);
            int index = (7 - j) * 8 + i;
            for (int piece = 0; piece < Piece_NB; piece++){
                for (int color = 0; color < Color_NB; color++){
                    auto bitboard = _board.getBitBoard(Piece(piece), Color(color));
                    const auto &pixmap = _pixmaps[piece * 2 + color];
                    if (bitboard & squareToBB(index)){
                        painter.drawPixmap(square - pieceMargin, pixmap);
                    }
                }
            }

            if (_mouseSelection == squareToBB(index)){
                painter.save();
                QRect square(i * _canvasRect.width() / 8 + _canvasRect.left(),
                             j * _canvasRect.width() / 8 + _canvasRect.top(),
                             ss, ss);
                painter.setBrush(QBrush(QColor("#88aa0000")));
                painter.drawRect(square);
                painter.restore();
            }

            if (_mobility & squareToBB(index)){
                painter.save();
                QRect square(i * _canvasRect.width() / 8 + _canvasRect.left(),
                             j * _canvasRect.width() / 8 + _canvasRect.top(),
                             ss, ss);
                painter.setBrush(QBrush(QColor("#8811ffaa")));
                painter.drawRect(square);
                painter.restore();
            }
        }
    }

    QFont f = font();
    f.setPixelSize(16);
    painter.setFont(f);
    // Draw rank & file
    for (int i = 0; i < 8; i++){
        int x = i * _canvasRect.width() / 8 + _canvasRect.left();
        int y = i * _canvasRect.width() / 8 + _canvasRect.top();
        int padding = _canvasRect.width() / 16;
        QString textRank = QString::number(8 - i);
        QString textFile = tr("ABCDEFGH").at(i);
        painter.drawText(x + padding, _canvasRect.bottom() + 16, textFile);
        painter.drawText(_canvasRect.left() - 16, y + padding, textRank);
    }

    auto val = eval::estimate(_board);
    painter.drawText(10, 20, tr("Score: %1").arg(val));

}

void ChessBoardView::mousePressEvent(QMouseEvent *event)
{
    auto pos = event->pos();
    if (event->button() == Qt::LeftButton){
        if (_canvasRect.contains(pos)){
            int ss = _canvasRect.width() / 8;
            int i = (pos.x() - _canvasRect.left()) / ss;
            int j = (pos.y() - _canvasRect.top()) / ss;
            int index = (7 - j) * 8 + i;
            auto bb = squareToBB(index);
            if (_mobility & bb){
                auto move = makeMove(bbToSquare(_mouseSelection), index);
                _board.doMove(move);
                qDebug() << "Chess board state" << toFenString(_board);
                qDebug() << "Value of board" << eval::estimate(_board);
                _mouseSelection = 0;
                _mobility = 0;
                emit boardChanged();
            } else {
                _mouseSelection = bb;
                _mobility = getMobility(_board, Square(index));
            }
            update();
        }
    } else {
        _mouseSelection = 0;
        _mobility = 0;
        update();
    }

    //    auto b = getMobility(board);
}

const kchess::ChessBoard &ChessBoardView::board() const
{
    return _board;
}

void ChessBoardView::setBoard(const kchess::ChessBoard &newBoard)
{
    _board = newBoard;
    update();
}
