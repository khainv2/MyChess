#include "define.h"
#include "attack.h"
#include <QDebug>

using namespace kc;

// Chuyển đổi bitboard thành string để debug và hiển thị
// Hiển thị bàn cờ với 'x' cho bit được set, '_' cho bit 0
std::string kc::bbToString(BB bb){
    std::string s;
    // Duyệt từ rank 8 xuống rank 1 (hiển thị như bàn cờ thật)
    for (int r = 7; r >= 0; r--){
        s += '\n';
        for (int c = 0; c < 8; c++){
            int idx = r * 8 + c;  // Tính index từ rank và file
            s += (bb & (1ULL << idx)) ? 'x' : '_';  // 'x' nếu bit được set
            s += ' ';  // Thêm space để dễ đọc
        }
    }
    // Alternative implementation (commented out):
    // for (int i = 0; i < 64; i++){
    //     s += (bb & 1) ? 'x' : '_';
    //     bb >>= 1;
    // }
    return s;
}

// Tạo mô tả text cho nước đi (dùng cho logging và debug)
// Format: "E2>E4", "E7>E8[P]Q" (promotion), "E5>F6 cap" (capture)
std::string kc::Move::getDescription() const noexcept {
    auto s = src();  // Ô xuất phát
    auto d = dst();  // Ô đích
    
    // Arrays chứa ký hiệu file và rank
    static std::string fileTxt[8] = { "A", "B", "C", "D", "E", "F", "G", "H" };
    static std::string rankTxt[8] = { "1", "2", "3", "4", "5", "6", "7", "8" };

    // Tính toán file và rank cho source và destination
    int srcFileIdx = s % 8;   // File = index % 8
    int srcRankIdx = s / 8;   // Rank = index / 8
    int dstFileIdx = d % 8;
    int dstRankIdx = d / 8;
    
    // Xử lý đặc biệt cho nước đi phong cấp
    if (is<Promotion>()){
        auto p = getPromotionPieceType();
        return fileTxt[srcFileIdx] + rankTxt[srcRankIdx]
             + ">" + fileTxt[dstFileIdx] + rankTxt[dstRankIdx]
             + "[P]" + pieceToChar(makePiece(White, PieceType(p)));  // [P]Q, [P]R, etc.
    }
    
    // Format cơ bản: "E2>E4"
    std::string out = fileTxt[srcFileIdx] + rankTxt[srcRankIdx]
            + ">" + fileTxt[dstFileIdx] + rankTxt[dstRankIdx];

    // Thêm " cap" nếu là nước bắt quân
    if (is<Capture>()){
        out += " cap";
    }
    return out;
}

// Chuyển đổi enum Piece thành ký tự đại diện
// White pieces: viết hoa (P, R, N, B, Q, K)
// Black pieces: viết thường (p, r, n, b, q, k)
char kc::pieceToChar(Piece piece){
    switch (piece){
    case WhitePawn: return 'P';    // Tốt trắng
    case WhiteRook: return 'R';    // Xe trắng
    case WhiteKnight: return 'N';  // Mã trắng (Knight)
    case WhiteBishop: return 'B';  // Tượng trắng
    case WhiteKing: return 'K';    // Vua trắng
    case WhiteQueen: return 'Q';   // Hậu trắng
    case BlackPawn: return 'p';    // Tốt đen
    case BlackRook: return 'r';    // Xe đen
    case BlackKnight: return 'n';  // Mã đen
    case BlackBishop: return 'b';  // Tượng đen
    case BlackKing: return 'k';    // Vua đen
    case BlackQueen: return 'q';   // Hậu đen
    default: return '.';           // Ô trống
    }
}

// Chuyển đổi ký tự thành enum Piece (ngược lại với pieceToChar)
// Dùng để parse FEN notation và input từ người dùng
Piece kc::charToPiece(char c){
    switch (c){
    case 'P': return WhitePawn;    // Tốt trắng
    case 'R': return WhiteRook;    // Xe trắng
    case 'N': return WhiteKnight;  // Mã trắng
    case 'B': return WhiteBishop;  // Tượng trắng
    case 'K': return WhiteKing;    // Vua trắng
    case 'Q': return WhiteQueen;   // Hậu trắng
    case 'p': return BlackPawn;    // Tốt đen
    case 'r': return BlackRook;    // Xe đen
    case 'n': return BlackKnight;  // Mã đen
    case 'b': return BlackBishop;  // Tượng đen
    case 'k': return BlackKing;    // Vua đen
    case 'q': return BlackQueen;   // Hậu đen
    default: return PieceNone;     // Ký tự không hợp lệ
    }

}

// Chuyển đổi enum Rank thành ký tự ('1' - '8')
char kc::rankToChar(Rank r)
{
    return '1' + r;  // Rank1 = 0 -> '1', Rank2 = 1 -> '2', ...
}

// Chuyển đổi ký tự thành enum Rank
Rank kc::charToRank(char c)
{
    return Rank(c - '1');  // '1' -> Rank1 = 0, '2' -> Rank2 = 1, ...
}

// Chuyển đổi enum File thành ký tự ('a' - 'h')
char kc::fileToChar(File f)
{
    return 'a' + f;  // FileA = 0 -> 'a', FileB = 1 -> 'b', ...
}

// Chuyển đổi ký tự thành enum File
File kc::charToFile(char c)
{
    return File(c - 'a');  // 'a' -> FileA = 0, 'b' -> FileB = 1, ...
}
