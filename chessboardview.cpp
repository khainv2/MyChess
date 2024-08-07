#include "chessboardview.h"
#include "promotionselectoindialog.h"
#include "algorithm/evaluation.h"
#include <QPainter>
#include <QDebug>
#include <algorithm/util.h>
#include <QMouseEvent>
#include <algorithm/evaluation.h>
#include <algorithm>

using namespace kc;
ChessBoardView::ChessBoardView(QWidget *parent) : QWidget(parent)
{    
    _pixmaps[WhitePawn] = QPixmap(":/w_pawn.png");
    _pixmaps[WhiteBishop] = QPixmap(":/w_bishop.png");
    _pixmaps[WhiteKnight] = QPixmap(":/w_knight.png");
    _pixmaps[WhiteRook] = QPixmap(":/w_rook.png");
    _pixmaps[WhiteQueen] = QPixmap(":/w_queen.png");
    _pixmaps[WhiteKing] = QPixmap(":/w_king.png");
    _pixmaps[BlackPawn] = QPixmap(":/b_pawn.png");
    _pixmaps[BlackBishop] = QPixmap(":/b_bishop.png");
    _pixmaps[BlackKnight] = QPixmap(":/b_knight.png");
    _pixmaps[BlackRook] = QPixmap(":/b_rook.png");
    _pixmaps[BlackQueen] = QPixmap(":/b_queen.png");
    _pixmaps[BlackKing] = QPixmap(":/b_king.png");
//    parseFENString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &_board);
//    parseFENString("r3k2r/pppp3p/4p1p1/5p2/2B5/4PN2/PPPP1PPP/R3K2R w KQkq - 0 1", &_board); // Has castling
//    parseFENString("1rnb2r1/ppPp2kp/4p1p1/5p2/2B5/4PN2/PPP2PPP/R3K2R w KQ - 0 1", &_board); // promotion

    parseFENString("1rnQ2r1/pp1p1k1p/4p1p1/5p2/2B5/4PN2/PPP2PPP/R3K2R w KQ - 1 2", &_board); // check
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
            auto piece = _board.pieces[index];
            if (piece != PieceNone){
                painter.drawPixmap(square - pieceMargin, _pixmaps[piece]);
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

            bool found = false;
            for (Move move: _moveList){
                if (move.dst() == index){
                    found = true;
                }
            }
            if (found){
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

            std::vector<Move> availables(_moveList.size());
            auto it = std::copy_if(_moveList.begin(), _moveList.end(),
                                  availables.begin(), [=](auto m){
                return m.dst() == index;
            });
            availables.resize(std::distance(availables.begin(), it));  // shrink container to new size

            if (availables.size()){
                // Kiểm tra với trường hợp promotion
                Move move;
                if (availables.size() > 1){
                    PromotionSelectionDialog dialog;
                    dialog.exec();
                    auto piece = dialog.type();
                    auto it = std::find_if(availables.begin(), availables.end(), [=](Move m){
                        return m.getPromotionPieceType() == piece;
                    });
                    move = *it;
                    // qDebug() << "Promotion to" << move.getPiecePromotion() << move.getDescription().c_str();
                } else {
                    move = availables.at(0);
                }
                _board.doMove(move);
                _mouseSelection = 0;
                _moveList.clear();
                emit boardChanged();

            } else {
                _mouseSelection = bb;
                _moveList = getMoveListForSquare(_board, Square(index));
            }
            update();
        }
    } else {
        _mouseSelection = 0;
        _moveList.clear();
        update();
    }

    //    auto b = getMobility(board);
}

const kc::Board &ChessBoardView::board() const
{
    return _board;
}

void ChessBoardView::setBoard(const kc::Board &newBoard)
{
    _board = newBoard;
    update();
}
