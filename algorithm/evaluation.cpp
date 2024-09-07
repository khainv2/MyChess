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
        for (int i = 0; i < Square_NB; i++){
            int flipI = flip(i);
            MG_Table[makePiece(White, pieceType)][i] = MG_Material[pieceType] + MG_Pieces[pieceType][i];
            EG_Table[makePiece(White, pieceType)][i] = EG_Material[pieceType] + EG_Pieces[pieceType][i];
            MG_Table[makePiece(Black, pieceType)][i] = MG_Material[pieceType] + MG_Pieces[pieceType][flipI];
            EG_Table[makePiece(Black, pieceType)][i] = EG_Material[pieceType] + EG_Pieces[pieceType][flipI];
        }
    }
}

int kc::eval::estimate(const Board &board)
{
    int mg[2] = { 0, 0 };
    int eg[2] = { 0, 0 };
    int gamePhase = 0;

    for (int sq = 0; sq < 64; ++sq) {
        auto pc = board.pieces[sq];
        if (pc != PieceNone){
            mg[pieceToColor(pc)] += MG_Table[pc][sq];
            eg[pieceToColor(pc)] += EG_Table[pc][sq];
            gamePhase += gamephaseInc[pc];
        }
    }

    int mgScore = mg[White] - mg[Black];
    int egScore = eg[White] - eg[Black];
    constexpr int MaxGamePhase = 24;
    int mgPhase = std::min(gamePhase, MaxGamePhase); // In case early promotion
    int egPhase = 24 - mgPhase;

    int mobility = 8 * (MoveGen::instance->countMoveList<White>(board)
                              - MoveGen::instance->countMoveList<Black>(board));

    return (mgScore * mgPhase + egScore * egPhase) / 24 + mobility;
}

int kc::eval::getMaterial(const Board &board) {
    u64 b = board.colors[Black], w = board.colors[White];
    return (popCount(board.types[Pawn] & w) - popCount(board.types[Pawn] & b)) * Value_Pawn
            + (popCount(board.types[Knight] & w) - popCount(board.types[Knight] & b)) * Value_Knight
            + (popCount(board.types[Bishop] & w) - popCount(board.types[Bishop] & b)) * Value_Bishop
            + (popCount(board.types[Rook] & w) - popCount(board.types[Rook] & b)) * Value_Rook
            + (popCount(board.types[Queen] & w) - popCount(board.types[Queen] & b)) * Value_Queen;
}
