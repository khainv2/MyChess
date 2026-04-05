#ifndef CHESSBOARDVIEW_H
#define CHESSBOARDVIEW_H

#include "algorithm/board.h"
#include "algorithm/movegen.h"

#include <QColor>
#include <QWidget>

struct MoveArrow {
    int src = -1;
    int dst = -1;
    QColor color;
};

class ChessBoardView : public QWidget
{
    Q_OBJECT
public:
    explicit ChessBoardView(QWidget *parent = nullptr);
    const kc::Board &board() const;
    void setBoard(const kc::Board &newBoard);

    void doMove(kc::Move move);
    void undoMove();

    void setArrows(const std::vector<MoveArrow> &arrows);
    void clearArrows();
    void setInteractive(bool interactive);

signals:
    void boardChanged();
    // QWidget interface
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    void initPixmaps();
    void drawArrow(QPainter &painter, int src, int dst, const QColor &color);

    QPixmap _pixmaps[16];
    int _borderSize = 30;
    QRect _canvasRect;
    QColor _boardColor = Qt::black;
    bool _interactive = true;

    kc::Board _board;
    std::vector<kc::Move> _moveAbility;
    std::vector<kc::Move> _moveList;
    std::vector<kc::BoardState *> _boardStates;
    kc::BB _mouseSelection = 0;

    std::vector<MoveArrow> _arrows;
};

#endif // CHESSBOARDVIEW_H
