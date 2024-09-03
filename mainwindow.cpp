#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "algorithm/engine.h"
#include "algorithm/util.h"

using namespace kc;
struct BoardValue {
    Move move;
    QString fen;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->chessBoard, &ChessBoardView::boardChanged, [=](){
        ui->label->setText(QString::fromStdString(toFenString(ui->chessBoard->board())));
    });

    on_bt_StartCalculate_clicked();
//    ui->chessBoard->setVisible(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_bt_StartCalculate_clicked()
{
    auto board = ui->chessBoard->board();

    std::vector<Move> moves;
    moves.resize(1000000);
    Move *movePtr = moves.data();

    int countTotal = 0;
    int count = MoveGen::instance->genMoveList(board, movePtr);
    countTotal += count;

    auto move = engine.calc(board);
    ui->chessBoard->doMove(move);

//    ui->chessBoard->setBoard(board);
}

void MainWindow::on_bt_ParseFen_clicked()
{
    qDebug() << "Parse fen" << ui->le_Fen->text();
    kc::Board cb;
    parseFENString(ui->le_Fen->text().toStdString(), &cb);
    ui->chessBoard->setBoard(cb);
}


void MainWindow::on_bt_Undo_clicked()
{
    ui->chessBoard->undoMove();
}

