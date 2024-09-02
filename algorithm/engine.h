#pragma once
#include "board.h"
#include "define.h"
namespace kc {

struct Node {
    int level = 0;
    std::vector<Node *> children;

    Board board;
    BoardState boardState;
    Move move = Move::NullMove;
    int score = 0;
    bool isBestMove;
    bool isSelected;

    ~Node(){
        while (children.size()){
            delete children.at(0);
            children.erase(children.begin());
        }
    }
};

class Tree {

};

class Engine {
public:
    Engine();
    Move calc(const Board &chessBoard);

    template <Color color, bool isRoot>
    int alphabeta(Node *node, Board &board, int depth, int alpha, int beta);
    Node *getRootNode() const;

private:
    Move bestMoves[256];
    int countBestMove = 0;
    int fixedDepth;

    Move bestMoveHistories[256][256]; // Chiều ngang tương đương với một số kết quả giống nhau, chiều dọc bao gồm list các nước theo depth giảm dần
    Move moves[256];
    int moveCount = 0;
    Move engineMoves[20][256];

    Node *rootNode = nullptr;
};
}
