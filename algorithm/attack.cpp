#include "attack.h"
#include <QDebug>
#include <bitset>
#include <random>
#include <set>
#include <vector>
#include <QElapsedTimer>

using namespace kc;

u64 kc::attack::kings[64] = { 0 };
u64 kc::attack::knights[64] = { 0 };
u64 kc::attack::pawns[2][64] = {};
u64 kc::attack::pawnPushes[2][64] = {};
u64 kc::attack::pawnPushes2[2][64] = {};
u64 kc::attack::rooks[64][262144] = { };
u64 kc::attack::bishops[64][262144] = { };

// Trả về các ô nằm giữa 2 ô để thuận tiện cho việc tính checkmask
BB kc::attack::between[Square_NB][Square_NB];

kc::attack::MagicBitboard kc::attack::rookMagicBitboards[64] = { };
kc::attack::MagicBitboard kc::attack::bishopMagicBitboards[64] = { };
void kc::attack::init()
{
    initMagicTable();
    for (int i = 0; i < 64; i++){
        u64 square = indexToBB(i);
        kings[i] = getKingAttacks(square);

        knights[i] = getKnightAttacks(square);

        pawns[White][i] = getPawnAttacks<White>(square);
        pawns[Black][i] = getPawnAttacks<Black>(square);
        pawnPushes[White][i] = oneSquareAttack<+8>(square, B_U);
        pawnPushes[Black][i] = oneSquareAttack<-8>(square, B_D);
        pawnPushes2[White][i] = oneSquareAttack<+16>(square, ~B_D2);
        pawnPushes2[Black][i] = oneSquareAttack<-16>(square, ~B_U2);
    }
    for (int i = 0; i < 64; i++){
        BB iBB = indexToBB(i);
        for (int j = 0; j < 64; j++){
            BB jBB = indexToBB(j);
            between[i][j] = jBB;
            BB occ = iBB | jBB;
            BB rookAtk = attack::getRookAttacks(i, occ);
            if (rookAtk & jBB){
                auto mask = (rookAtk & attack::getRookAttacks(j, occ)) | jBB;
                between[i][j] = mask;
            }
            BB bishopAtk = attack::getBishopAttacks(i, occ);
            if (bishopAtk & jBB){
                auto mask = (bishopAtk & attack::getBishopAttacks(j, occ)) | jBB;
                between[i][j] = mask;
            }
        }
    }
}

void attack::initMagicTable()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    std::vector<int> vectorTestCollition(262144);
    auto vectorTestCollitionPtr = vectorTestCollition.data();
    QElapsedTimer timer;
    timer.start();

    // Tìm kiếm số magic cho các quân xe
    for (int i = 0; i < 64; i++){
        u64 sqbb = indexToBB(i);
        u64 mask = ((Rank1_BB << (i / 8 * 8)) | (FileA_BB << (i % 8))) & (~sqbb);
        rookMagicBitboards[i].mask = mask;

        // Tìm kiếm index của các bit trên mask được bật
        int bitMaskIndexes[14];
        int numBitOn = 0;
        for (int j = 0; j < 64; j++){
            if ((mask >> j) & 1){
                bitMaskIndexes[numBitOn] = j;
                numBitOn++;
            }
        }
        // Tìm kiếm ngẫu nhiên số magic
        u64 magic;
        while (true){
            magic = dis(gen);
            bool isCollition = false;
            std::fill(vectorTestCollition.begin(), vectorTestCollition.end(), 0);
            for (int j = 0; j < 16384; j++){
                u64 test = 0;
                for (int k = 0; k < 14; k++){
                    if ((j >> k) & 1){
                        test += (1ULL << bitMaskIndexes[k]);
                    }
                }
                int index = (test * magic) >> RookMagicShiftLength;
                if (vectorTestCollitionPtr[index]){
                    isCollition = true;
                    break;
                }
                vectorTestCollitionPtr[index] = 1;
            }
            if (!isCollition){
                break;
            }
        }

        // Tính toán kết quả đối với từng occupancy
        for (int j = 0; j < 16384; j++){
            u64 occ = 0;
            for (int k = 0; k < 14; k++){
                if ((j >> k) & 1){
                    occ += (1ULL << bitMaskIndexes[k]);
                }
            }
            int index = (occ * magic) >> RookMagicShiftLength;
            rooks[i][index] = getSlideAttack(sqbb, +1, occ | B_R)
                            | getSlideAttack(sqbb, -1, occ | B_L)
                            | getSlideAttack(sqbb, +8, occ | B_U)
                            | getSlideAttack(sqbb, -8, occ | B_D);
        }
        rookMagicBitboards[i].magic = magic;
    }

    qDebug() << "Found all magic for rook" << (timer.nsecsElapsed() / 1000000) << "ms";
    timer.restart();
    for (int i = 0; i < 64; i++){
        u64 sqbb = indexToBB(i);
        u64 mask = getSlideAttack(sqbb, +7, B_U | B_L)
                | getSlideAttack(sqbb, -7, B_D | B_R)
                | getSlideAttack(sqbb, +9, B_U | B_R)
                | getSlideAttack(sqbb, -9, B_D | B_L);

        bishopMagicBitboards[i].mask = mask;

        // Tìm kiếm index của các bit trên mask được bật
        int bitMaskIndexes[13] = { -1 };
        int numBitOn = 0;
        for (int j = 0; j < 64; j++){
            if ((mask >> j) & 1){
                bitMaskIndexes[numBitOn] = j;
                numBitOn++;
            }
        }
        // Tìm kiếm ngẫu nhiên số magic
//        vectorTestCollition.resize(pow2(13));
        u64 magic;
        while (true){
            magic = dis(gen);
            bool isCollition = false;
            std::fill(vectorTestCollition.begin(), vectorTestCollition.end(), 0);
            for (int j = 0; j < pow2(numBitOn); j++){
                u64 test = 0;
                for (int k = 0; k < numBitOn; k++){
                    if (k >= 0 && (j >> k) & 1){
                        test += (1ULL << bitMaskIndexes[k]);
                    }
                }
//                qDebug() << "Test val" << QString::number(test, 2);
                int index = (test * magic) >> BishopMagicShiftLength;
                if (vectorTestCollitionPtr[index]){
                    isCollition = true;
                    break;
                }
                vectorTestCollitionPtr[index] = 1;
            }

            if (!isCollition){
                break;
            }
        }

        // Tính toán kết quả đối với từng occupancy
        for (int j = 0; j < pow2(numBitOn); j++){
            u64 occ = 0;
            for (int k = 0; k < numBitOn; k++){
                if ((j >> k) & 1){
                    occ += (1ULL << bitMaskIndexes[k]);
                }
            }
            int index = (occ * magic) >> BishopMagicShiftLength;
            bishops[i][index] = getSlideAttack(sqbb, +9, occ | B_R | B_U)
                            | getSlideAttack(sqbb, -9, occ | B_L | B_D)
                            | getSlideAttack(sqbb, +7, occ | B_L | B_U)
                            | getSlideAttack(sqbb, -7, occ | B_R | B_D);
        }
        bishopMagicBitboards[i].magic = magic;
    }

    qDebug() << "Found all magic for bishop" << (timer.nsecsElapsed() / 1000000) << "ms";
}






















