#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "algorithm/engine.h"
#include "algorithm/util.h"

using namespace kchess;
struct BoardValue {
    Move move = 0;
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
    int count;
    generateMoveList(board, movePtr, count);
    countTotal += count;

//    QList<BoardValue> states;
//    BoardValue val;
//    val.fen = toFenString(board);
//    fens.append()

    for (int i = 0; i < count; i++){
        Board newBoard = board;
        newBoard.doMove(movePtr[i]);
        int ncount;
        generateMoveList(newBoard, movePtr + countTotal, ncount);
        for (int j = 0; j < ncount; j++){
            Board boardJ = newBoard;
            boardJ.doMove((movePtr + countTotal)[j]);

        }
        countTotal += ncount;
    }




//    kchess::Engine engine;
//    auto move = engine.calc(board);
//    board.doMove(move);
//    ui->chessBoard->setBoard(board);
}

void MainWindow::on_bt_ParseFen_clicked()
{
    auto boardView = new ChessBoardView;
    kchess::Board cb;
    parseFENString(ui->le_Fen->text().toStdString(), &cb);
    boardView->setBoard(cb);
    boardView->show();
}

