#pragma once

#include <QWidget>
#include <algorithm/engine.h>

class TreeGraph : public QWidget
{
    Q_OBJECT

    enum {
        Padding = 8,
        Spacing = 16,
        NodeWidth = 120,
        NodeHeight = 48,
    };
public:
    explicit TreeGraph(QWidget *parent = nullptr);

    void setRootNode(kc::Node *newRootNode);

private:
    void drawNode(QPainter &painter, const QPoint &pos, kc::Node *node);
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
signals:

private:
    kc::Node *rootNode = nullptr;
};

