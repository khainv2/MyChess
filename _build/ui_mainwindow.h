/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.12.12
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QWidget>
#include "chessboardview.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout;
    QWidget *widget;
    QGridLayout *gridLayout;
    QLineEdit *le_Fen;
    QLabel *label;
    QPushButton *bt_ParseFen;
    QPushButton *bt_Undo;
    QPushButton *bt_StartCalculate;
    QTreeWidget *treeWidget;
    ChessBoardView *chessBoard;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(692, 367);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        horizontalLayout = new QHBoxLayout(centralwidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        widget = new QWidget(centralwidget);
        widget->setObjectName(QString::fromUtf8("widget"));
        widget->setMinimumSize(QSize(300, 0));
        widget->setMaximumSize(QSize(300, 16777215));
        gridLayout = new QGridLayout(widget);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        le_Fen = new QLineEdit(widget);
        le_Fen->setObjectName(QString::fromUtf8("le_Fen"));

        gridLayout->addWidget(le_Fen, 1, 0, 1, 1);

        label = new QLabel(widget);
        label->setObjectName(QString::fromUtf8("label"));
        label->setMinimumSize(QSize(0, 40));
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByMouse);

        gridLayout->addWidget(label, 3, 0, 1, 1);

        bt_ParseFen = new QPushButton(widget);
        bt_ParseFen->setObjectName(QString::fromUtf8("bt_ParseFen"));

        gridLayout->addWidget(bt_ParseFen, 2, 0, 1, 1);

        bt_Undo = new QPushButton(widget);
        bt_Undo->setObjectName(QString::fromUtf8("bt_Undo"));

        gridLayout->addWidget(bt_Undo, 4, 0, 1, 1);

        bt_StartCalculate = new QPushButton(widget);
        bt_StartCalculate->setObjectName(QString::fromUtf8("bt_StartCalculate"));

        gridLayout->addWidget(bt_StartCalculate, 0, 0, 1, 1);

        treeWidget = new QTreeWidget(widget);
        treeWidget->setObjectName(QString::fromUtf8("treeWidget"));

        gridLayout->addWidget(treeWidget, 5, 0, 1, 1);


        horizontalLayout->addWidget(widget);

        chessBoard = new ChessBoardView(centralwidget);
        chessBoard->setObjectName(QString::fromUtf8("chessBoard"));
        chessBoard->setMinimumSize(QSize(0, 282));

        horizontalLayout->addWidget(chessBoard);

        MainWindow->setCentralWidget(centralwidget);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "MainWindow", nullptr));
        le_Fen->setText(QApplication::translate("MainWindow", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", nullptr));
        label->setText(QApplication::translate("MainWindow", "TextLabel", nullptr));
        bt_ParseFen->setText(QApplication::translate("MainWindow", "Parse FEN", nullptr));
        bt_Undo->setText(QApplication::translate("MainWindow", "Undo", nullptr));
        bt_StartCalculate->setText(QApplication::translate("MainWindow", "Start calculate", nullptr));
        QTreeWidgetItem *___qtreewidgetitem = treeWidget->headerItem();
        ___qtreewidgetitem->setText(1, QApplication::translate("MainWindow", "Score", nullptr));
        ___qtreewidgetitem->setText(0, QApplication::translate("MainWindow", "Move", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
