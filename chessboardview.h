#ifndef CHESSBOARDVIEW_H
#define CHESSBOARDVIEW_H

#include "algorithm/chessboard.h"
#include "algorithm/movegenerator.h"

#include <QColor>
#include <QWidget>

class ChessBoardView : public QWidget
{
    Q_OBJECT
public:
    explicit ChessBoardView(QWidget *parent = nullptr);
    const kchess::ChessBoard &board() const;
    void setBoard(const kchess::ChessBoard &newBoard);

signals:
    // QWidget interface
protected:
    void paintEvent(QPaintEvent *event) override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
private:
    QPixmap _pixmaps[16];
private:
    int _borderSize = 30;
    QRect _canvasRect;
    QColor _boardColor = Qt::black;

    kchess::ChessBoard _board;
    kchess::Bitboard _mobility = 0;
    kchess::Bitboard _mouseSelection = 0;
};

#endif // CHESSBOARDVIEW_H
