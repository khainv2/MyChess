#include "evaluation.h"
#include "movegen.h"
#include "attack.h"
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
kc::BB kc::eval::PassedPawnMask[Color_NB][Square_NB] = {};

// ── Precompute passed pawn masks ─────────────────────────────────────
// Mask chứa tất cả ô mà tốt đối phương có thể chặn/bắt trên đường phong cấp
// Bao gồm: cùng file + 2 file liền kề, tất cả rank phía trước
static void initPassedPawnMasks() {
    for (int sq = 0; sq < 64; ++sq) {
        int file = sq & 7;
        int rank = sq >> 3;

        kc::BB mask = 0;
        // White passed pawn: kiểm tra các rank phía trên (rank+1 đến 7)
        for (int r = rank + 1; r <= 7; ++r) {
            if (file > 0) mask |= kc::indexToBB(r * 8 + file - 1);
            mask |= kc::indexToBB(r * 8 + file);
            if (file < 7) mask |= kc::indexToBB(r * 8 + file + 1);
        }
        kc::eval::PassedPawnMask[kc::White][sq] = mask;

        mask = 0;
        // Black passed pawn: kiểm tra các rank phía dưới (rank-1 đến 0)
        for (int r = rank - 1; r >= 0; --r) {
            if (file > 0) mask |= kc::indexToBB(r * 8 + file - 1);
            mask |= kc::indexToBB(r * 8 + file);
            if (file < 7) mask |= kc::indexToBB(r * 8 + file + 1);
        }
        kc::eval::PassedPawnMask[kc::Black][sq] = mask;
    }
}

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
    initPassedPawnMasks();
}

// ── Passed Pawns ─────────────────────────────────────────────────────
// Tốt thông (passed pawn): không có tốt đối phương nào chặn hoặc tấn công
// trên đường đi đến hàng phong cấp
template<kc::Color color>
int kc::eval::evalPassedPawns(const Board &board) {
    constexpr Color enemy = !color;
    BB myPawns = board.getPieceBB<color, Pawn>();
    BB enemyPawns = board.getPieceBB<enemy, Pawn>();
    int mgBonus = 0, egBonus = 0;

    while (myPawns) {
        Square sq = popLsb(myPawns);
        // Nếu không có tốt đối phương nào trong vùng chặn → passed pawn
        if (!(PassedPawnMask[color][sq] & enemyPawns)) {
            Rank r = (color == White) ? getRank(sq) : Rank(7 - getRank(sq));
            mgBonus += PassedPawnBonus_MG[r];
            egBonus += PassedPawnBonus_EG[r];
        }
    }
    return mgBonus; // Caller sẽ dùng cả mg và eg riêng
}

// ── King Safety ──────────────────────────────────────────────────────
// Đánh giá mức độ an toàn của vua: đếm quân tấn công vào vùng quanh vua đối phương
// Penalty lớn hơn khi nhiều quân cùng nhắm vào vua
template<kc::Color color>
int kc::eval::evalKingSafety(const Board &board) {
    constexpr Color enemy = !color;
    // Vùng xung quanh vua đối phương (king zone)
    Square enemyKingSq = lsb(board.getPieceBB<enemy, King>());
    BB kingZone = attack::kings[enemyKingSq] | indexToBB(enemyKingSq);

    BB occ = board.getOccupancy();
    int attackerCount = 0;
    int attackWeight = 0;

    // Đếm quân tấn công vào king zone
    // Knight
    BB knights = board.getPieceBB<color, Knight>();
    while (knights) {
        Square sq = popLsb(knights);
        if (attack::knights[sq] & kingZone) {
            attackerCount++;
            attackWeight += KingAttackWeight[Knight];
        }
    }
    // Bishop
    BB bishops = board.getPieceBB<color, Bishop>();
    while (bishops) {
        Square sq = popLsb(bishops);
        if (attack::getBishopAttacks(sq, occ) & kingZone) {
            attackerCount++;
            attackWeight += KingAttackWeight[Bishop];
        }
    }
    // Rook
    BB rooks = board.getPieceBB<color, Rook>();
    while (rooks) {
        Square sq = popLsb(rooks);
        if (attack::getRookAttacks(sq, occ) & kingZone) {
            attackerCount++;
            attackWeight += KingAttackWeight[Rook];
        }
    }
    // Queen
    BB queens = board.getPieceBB<color, Queen>();
    while (queens) {
        Square sq = popLsb(queens);
        if (attack::getQueenAttacks(sq, occ) & kingZone) {
            attackerCount++;
            attackWeight += KingAttackWeight[Queen];
        }
    }

    if (attackerCount < 2) return 0; // Cần ít nhất 2 quân tấn công mới nguy hiểm

    int safetyIdx = std::min(attackerCount, 8);
    // Kết hợp trọng số quân + bảng phi tuyến theo số attacker
    return attackWeight * KingSafetyTable[safetyIdx] / 100;
}

// ── Main evaluation ──────────────────────────────────────────────────
int kc::eval::estimate(const Board &board) {
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

    // ── Passed Pawns (tapered) ──
    {
        BB whitePawns = board.getPieceBB<White, Pawn>();
        BB blackPawns = board.getPieceBB<Black, Pawn>();
        // White passed pawns
        BB wp = whitePawns;
        while (wp) {
            Square sq = popLsb(wp);
            if (!(PassedPawnMask[White][sq] & blackPawns)) {
                Rank r = getRank(sq);
                mg[White] += PassedPawnBonus_MG[r];
                eg[White] += PassedPawnBonus_EG[r];
            }
        }
        // Black passed pawns
        BB bp = blackPawns;
        while (bp) {
            Square sq = popLsb(bp);
            if (!(PassedPawnMask[Black][sq] & whitePawns)) {
                Rank r = Rank(7 - getRank(sq));  // Đảo rank cho Black
                mg[Black] += PassedPawnBonus_MG[r];
                eg[Black] += PassedPawnBonus_EG[r];
            }
        }
    }

    // ── Bishop Pair ──
    if (popCount(board.getPieceBB<White, Bishop>()) >= 2) {
        mg[White] += BishopPairBonus_MG;
        eg[White] += BishopPairBonus_EG;
    }
    if (popCount(board.getPieceBB<Black, Bishop>()) >= 2) {
        mg[Black] += BishopPairBonus_MG;
        eg[Black] += BishopPairBonus_EG;
    }

    // ── King Safety (chỉ áp dụng trong middlegame) ──
    int kingSafety = 0;
    if (gamePhase > 6) { // Chỉ tính khi còn đủ quân (middlegame)
        // White tấn công vua Black → bonus cho White
        kingSafety += evalKingSafety<White>(board);
        // Black tấn công vua White → bonus cho Black
        kingSafety -= evalKingSafety<Black>(board);
    }

    int mgScore = mg[White] - mg[Black];
    int egScore = eg[White] - eg[Black];
    constexpr int MaxGamePhase = 24;
    int mgPhase = std::min(gamePhase, MaxGamePhase);
    int egPhase = 24 - mgPhase;

    int mobility = 8 * (MoveGen::instance->countMoveList<White>(board)
                              - MoveGen::instance->countMoveList<Black>(board));

    // King safety scale theo game phase (mạnh ở middlegame, yếu ở endgame)
    int scaledKingSafety = (kingSafety * mgPhase) / 24;

    return (mgScore * mgPhase + egScore * egPhase) / 24 + mobility + scaledKingSafety;
}

int kc::eval::estimateFast(const Board &board) {
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
    int mgPhase = std::min(gamePhase, MaxGamePhase);
    int egPhase = 24 - mgPhase;

    return (mgScore * mgPhase + egScore * egPhase) / 24;
}

int kc::eval::getMaterial(const Board &board) {
    u64 b = board.colors[Black], w = board.colors[White];
    return (popCount(board.types[Pawn] & w) - popCount(board.types[Pawn] & b)) * Value_Pawn
            + (popCount(board.types[Knight] & w) - popCount(board.types[Knight] & b)) * Value_Knight
            + (popCount(board.types[Bishop] & w) - popCount(board.types[Bishop] & b)) * Value_Bishop
            + (popCount(board.types[Rook] & w) - popCount(board.types[Rook] & b)) * Value_Rook
            + (popCount(board.types[Queen] & w) - popCount(board.types[Queen] & b)) * Value_Queen;
}

// Explicit template instantiation
template int kc::eval::evalPassedPawns<kc::White>(const kc::Board &board);
template int kc::eval::evalPassedPawns<kc::Black>(const kc::Board &board);
template int kc::eval::evalKingSafety<kc::White>(const kc::Board &board);
template int kc::eval::evalKingSafety<kc::Black>(const kc::Board &board);
