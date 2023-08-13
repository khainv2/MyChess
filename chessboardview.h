#ifndef CHESSBOARDVIEW_H
#define CHESSBOARDVIEW_H

#include "algorithm/define.h"
#include "algorithm/movegenerator.h"

#include <QColor>
#include <QWidget>

class ChessBoardView : public QWidget
{
    Q_OBJECT
public:
    explicit ChessBoardView(QWidget *parent = nullptr);

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
    QRect canvasRect;
    QColor _boardColor = Qt::black;

    kchess::ChessBoard board;
    kchess::Bitboard mobility = 0;
    kchess::Bitboard mouseSelection = 0;
};

#endif // CHESSBOARDVIEW_H
