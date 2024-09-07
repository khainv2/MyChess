#include "evaluation.h"
#include "movegen.h"
using namespace kc::eval;

const int* kc::eval::MG_Pieces[PieceType_NB] = {
    All_None,
    MG_Pawn,
    MG_Knight,
    MG_Bishop,
    MG_Rook,
    MG_Queen,
    MG_King
};

const int* kc::eval::EG_Pieces[PieceType_NB] = {
    All_None,
    EG_Pawn,
    EG_Knight,
    EG_Bishop,
    EG_Rook,
    EG_Queen,
    EG_King
};
int kc::eval::MG_Table[Piece_NB][Square_NB] = {};
int kc::eval::EG_Table[Piece_NB][Square_NB] = {};

void kc::eval::init() {
    for (int i = 0; i < Piece_NB; i++){
        for (int j = 0; j < Square_NB; j++){
            MG_Table[i][j] = 0;
            EG_Table[i][j] = 0;
        }
    }
    for (auto pieceType: { Pawn, Knight, Bishop, Rook, Queen, King }){
        for (int sq = 0; sq < Square_NB; sq++){
            int sqflip = flip(sq);
            MG_Table[makePiece(White, pieceType)][sq] = MG_Material[pieceType] + MG_Pieces[pieceType][sqflip];
            EG_Table[makePiece(White, pieceType)][sq] = EG_Material[pieceType] + EG_Pieces[pieceType][sqflip];
            MG_Table[makePiece(Black, pieceType)][sq] = MG_Material[pieceType] + MG_Pieces[pieceType][sq];
            EG_Table[makePiece(Black, pieceType)][sq] = EG_Material[pieceType] + EG_Pieces[pieceType][sq];
        }
    }
}

int kc::eval::estimate(const Board &board)
{
    int mg[2] = { 0, 0 };
    int eg[2] = { 0, 0 };
    int gamePhase = 0;

    QString str;
    for (int sq = 0; sq < 64; ++sq) {
        auto pc = board.pieces[sq];
        if (pc != PieceNone) {
            mg[pieceToColor(pc)] += MG_Table[pc][sq];
            eg[pieceToColor(pc)] += EG_Table[pc][sq];
//            str.append()
            gamePhase += gamephaseInc[pc];
        }
    }

    int mgScore = mg[White] - mg[Black];
    int egScore = eg[White] - eg[Black];
    int mgPhase = gamePhase;
    if (mgPhase > 24) mgPhase = 24; /* in case of early promotion */
    int egPhase = 24 - mgPhase;
    return (mgScore * mgPhase + egScore * egPhase) / 24;


//    u64 b = board.colors[Black], w = board.colors[White];
//    const BB b = board.colors[Black];
//    const BB w = board.colors[White];
//    const BB pawns = board.types[Pawn];
//    const BB knights = board.types[Knight];
//    const BB bishops = board.types[Bishop];
//    const BB rooks = board.types[Rook];
//    const BB queens = board.types[Queen];
//    const BB kings = board.types[King];

//    int material = (popCount(pawns & w) - popCount(pawns & b)) * Value_Pawn
//            + (popCount(knights & w) - popCount(knights & b)) * Value_Knight
//            + (popCount(bishops & w) - popCount(bishops & b)) * Value_Bishop
//            + (popCount(rooks & w) - popCount(rooks & b)) * Value_Rook
//            + (popCount(queens & w) - popCount(queens & b)) * Value_Queen;

//    int mobility = 10 * (MoveGen::instance->countMoveList<White>(board)
//                              - MoveGen::instance->countMoveList<Black>(board));

//    int
//    for (int i = 0; i < 64; i++){

//    }


//    auto wp = board.types[Pawn] & w;
//    auto bp = board.types[Pawn] & b;
    // Count isolated pawn



//    return material + mobility;
}

int kc::eval::getMaterial(const Board &board) {
    u64 b = board.colors[Black], w = board.colors[White];
    return (popCount(board.types[Pawn] & w) - popCount(board.types[Pawn] & b)) * Value_Pawn
            + (popCount(board.types[Knight] & w) - popCount(board.types[Knight] & b)) * Value_Knight
            + (popCount(board.types[Bishop] & w) - popCount(board.types[Bishop] & b)) * Value_Bishop
            + (popCount(board.types[Rook] & w) - popCount(board.types[Rook] & b)) * Value_Rook
            + (popCount(board.types[Queen] & w) - popCount(board.types[Queen] & b)) * Value_Queen;
}
