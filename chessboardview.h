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
    const kc::Board &board() const;
    void setBoard(const kc::Board &newBoard);

    void undoMove();
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

    kc::Board _board;
//    kchess::BB _mobility = 0;
    std::vector<kc::Move> _moveAbility;
    std::vector<kc::Move> _moveList;
    std::vector<kc::BoardState> _boardStates;
    kc::BB _mouseSelection = 0;
};

#endif // CHESSBOARDVIEW_H
