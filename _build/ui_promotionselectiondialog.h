/********************************************************************************
** Form generated from reading UI file 'promotionselectiondialog.ui'
**
** Created by: Qt User Interface Compiler version 5.12.12
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PROMOTIONSELECTIONDIALOG_H
#define UI_PROMOTIONSELECTIONDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>

QT_BEGIN_NAMESPACE

class Ui_PromotionSelectoinDialog
{
public:
    QRadioButton *rb_Queen;
    QRadioButton *rb_Rook;
    QRadioButton *rb_Bishop;
    QRadioButton *rb_Knight;
    QPushButton *bt_Accept;

    void setupUi(QDialog *PromotionSelectoinDialog)
    {
        if (PromotionSelectoinDialog->objectName().isEmpty())
            PromotionSelectoinDialog->setObjectName(QString::fromUtf8("PromotionSelectoinDialog"));
        PromotionSelectoinDialog->resize(239, 288);
        rb_Queen = new QRadioButton(PromotionSelectoinDialog);
        rb_Queen->setObjectName(QString::fromUtf8("rb_Queen"));
        rb_Queen->setGeometry(QRect(70, 40, 161, 23));
        rb_Queen->setChecked(true);
        rb_Rook = new QRadioButton(PromotionSelectoinDialog);
        rb_Rook->setObjectName(QString::fromUtf8("rb_Rook"));
        rb_Rook->setGeometry(QRect(70, 80, 161, 23));
        rb_Bishop = new QRadioButton(PromotionSelectoinDialog);
        rb_Bishop->setObjectName(QString::fromUtf8("rb_Bishop"));
        rb_Bishop->setGeometry(QRect(70, 120, 161, 23));
        rb_Knight = new QRadioButton(PromotionSelectoinDialog);
        rb_Knight->setObjectName(QString::fromUtf8("rb_Knight"));
        rb_Knight->setGeometry(QRect(70, 160, 161, 23));
        bt_Accept = new QPushButton(PromotionSelectoinDialog);
        bt_Accept->setObjectName(QString::fromUtf8("bt_Accept"));
        bt_Accept->setGeometry(QRect(30, 210, 181, 41));

        retranslateUi(PromotionSelectoinDialog);

        QMetaObject::connectSlotsByName(PromotionSelectoinDialog);
    } // setupUi

    void retranslateUi(QDialog *PromotionSelectoinDialog)
    {
        PromotionSelectoinDialog->setWindowTitle(QApplication::translate("PromotionSelectoinDialog", "Dialog", nullptr));
        rb_Queen->setText(QApplication::translate("PromotionSelectoinDialog", "Queen", nullptr));
        rb_Rook->setText(QApplication::translate("PromotionSelectoinDialog", "Rook", nullptr));
        rb_Bishop->setText(QApplication::translate("PromotionSelectoinDialog", "Bishop", nullptr));
        rb_Knight->setText(QApplication::translate("PromotionSelectoinDialog", "Knight", nullptr));
        bt_Accept->setText(QApplication::translate("PromotionSelectoinDialog", "OK", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PromotionSelectoinDialog: public Ui_PromotionSelectoinDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PROMOTIONSELECTIONDIALOG_H
