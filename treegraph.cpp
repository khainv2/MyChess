#include "treegraph.h"

#include <QPainter>
#include <QDebug>

using namespace kc;
TreeGraph::TreeGraph(QWidget *parent) : QWidget(parent)
{
    setFixedSize(500, 500);

    rootNode = new Node;
}

void TreeGraph::paintEvent(QPaintEvent *event)
{
    if (rootNode == nullptr){
        return;
    }
    QPainter painter(this);

    auto levelToHeight = [=](int level) -> int {
        return Padding + level * (NodeWidth + Spacing);
    };
    // Pos of root
    QPoint rootPos((width() - NodeWidth) / 2, levelToHeight(0));
    drawNode(painter, rootPos, rootNode);

    // Draw the children nodes
    for (int i = 0; i < rootNode->children.size(); i++){
        painter.drawText(0, i * 20, "Child Node");
    }
    
}

void TreeGraph::mousePressEvent(QMouseEvent *event)
{

}

void TreeGraph::mouseReleaseEvent(QMouseEvent *event)
{

}

void TreeGraph::mouseMoveEvent(QMouseEvent *event)
{

}

void TreeGraph::setRootNode(kc::Node *newRootNode)
{
    rootNode = newRootNode;
    qDebug() << "Root Node" << rootNode->children.size();
    update();
}

void TreeGraph::drawNode(QPainter &painter, const QPoint &pos, kc::Node *node)
{
    painter.setPen(Qt::black);
    QRect rect(pos, QSize(NodeWidth, NodeHeight));
    painter.drawRect(rect);

    QString text;
    if (node->level == 0){
        text = "Root";
    } else {
        text = QString::fromStdString(node->move.getDescription());
    }
    QFontMetrics metric(painter.font());
    auto r = metric.boundingRect(text);
    int x = rect.x() + rect.width() / 2 - r.width() / 2;
    int y = rect.y() + rect.height() / 2 + r.height() / 2;
    painter.drawText(x, y, text);
}
