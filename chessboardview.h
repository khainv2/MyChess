#ifndef CHESSBOARDVIEW_H
#define CHESSBOARDVIEW_H

#include "algorithm/board.h"
#include "algorithm/movegen.h"

#include <QColor>
#include <QWidget>

class ChessBoardView : public QWidget
{
    Q_OBJECT
public:
    explicit ChessBoardView(QWidget *parent = nullptr);
    const kchess::Board &board() const;
    void setBoard(const kchess::Board &newBoard);

signals:
    void boardChanged();
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

    kchess::Board _board;
//    kchess::BB _mobility = 0;
    std::vector<kchess::Move> _moveList;
    kchess::BB _mouseSelection = 0;
};

#endif // CHESSBOARDVIEW_H
