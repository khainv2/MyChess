#include "promotionselectiondialog.h"
#include "ui_promotionselectiondialog.h"

PromotionSelectionDialog::PromotionSelectionDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PromotionSelectoinDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
}

PromotionSelectionDialog::~PromotionSelectionDialog()
{
    delete ui;
}

kc::PieceType PromotionSelectionDialog::type()
{
    if (ui->rb_Rook->isChecked()) return kc::Rook;
    if (ui->rb_Knight->isChecked()) return kc::Knight;
    if (ui->rb_Bishop->isChecked()) return kc::Bishop;
    return kc::Queen;
}

void PromotionSelectionDialog::on_bt_Accept_clicked()
{
    accept();
}

