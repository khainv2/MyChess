#ifndef PROMOTIONSELECTOINDIALOG_H
#define PROMOTIONSELECTOINDIALOG_H

#include <QDialog>
#include <algorithm/define.h>

namespace Ui {
class PromotionSelectoinDialog;
}

class PromotionSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PromotionSelectionDialog(QWidget *parent = nullptr);
    ~PromotionSelectionDialog();

    kc::PieceType type();
private slots:
    void on_bt_Accept_clicked();

private:
    Ui::PromotionSelectoinDialog *ui;
};

#endif // PROMOTIONSELECTOINDIALOG_H
