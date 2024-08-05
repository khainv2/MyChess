#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "algorithm/engine.h"
#include "util.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_bt_StartCalculate_clicked()
{
    auto board = ui->chessBoard->board();
    kchess::Engine engine;
    auto move = engine.calc(board);
    board.doMove(move);
    ui->chessBoard->setBoard(board);
}

void MainWindow::on_bt_ParseFen_clicked()
{
    auto boardView = new ChessBoardView;
    kchess::ChessBoard cb;
    parseFENString(ui->le_Fen->text(), &cb);
    boardView->setBoard(cb);
    boardView->show();
}

