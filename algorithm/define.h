#pragma once
#ifndef __linux__
#include <intrin.h>
#endif
#include <string>

namespace kc {
// Định nghĩa các kiểu dữ liệu cơ bản cho chess engine
using u8 = unsigned char;      // 8-bit unsigned integer
using i16 = short;             // 16-bit signed integer  
using u16 = unsigned short;    // 16-bit unsigned integer
using u32 = unsigned int;      // 32-bit unsigned integer
using i64 = long long;         // 64-bit signed integer
using u64 = unsigned long long;// 64-bit unsigned integer (dùng cho bitboard)

// Hàm tính lũy thừa 2 tại compile time
constexpr u64 pow2(int v) noexcept {
    return v == 0 ? 1 : pow2(v - 1) * 2;
}

// Giá trị vô cực dùng trong alpha-beta pruning
constexpr int Infinity = 32002;

// Enum màu quân cờ
enum Color: u8 {
    White, Black, Color_NB
};
// Operator đảo màu (!White = Black, !Black = White)
static constexpr Color operator!(Color c){
    return Color(!int(c));
}
// Enum loại quân cờ (không phân biệt màu)
enum PieceType : u8 {
    PieceTypeNone = 0,
    Pawn, Knight, Bishop, Rook, Queen, King, PieceType_NB = 8
};

// Enum quân cờ (có phân biệt màu)
// Encoding: White pieces = 1-6, Black pieces = 9-14
enum Piece : u8 {
    PieceNone = 0,
    WhitePawn, WhiteKnight, WhiteBishop, WhiteRook, WhiteQueen, WhiteKing,
    BlackPawn = WhitePawn + 8, BlackKnight, BlackBishop, BlackRook, BlackQueen, BlackKing,
    Piece_NB = 16
};
// Trích xuất loại quân từ piece (loại bỏ thông tin màu)
constexpr PieceType pieceToType(Piece piece) noexcept {
    return PieceType(piece % PieceType_NB);
}
// Trích xuất màu quân từ piece
constexpr Color pieceToColor(Piece piece) noexcept {
    return Color(piece / PieceType_NB);
}
// Tạo piece từ màu và loại quân
constexpr Piece makePiece(Color color, PieceType type) noexcept {
    return Piece(color * PieceType_NB + type);
}
// Template version cho compile-time optimization
template <Color color, PieceType type>
constexpr Piece makePiece() noexcept {
    return Piece(color * PieceType_NB + type);
}

// Hàm chuyển đổi piece sang ký tự và ngược lại
char pieceToChar(Piece piece);
Piece charToPiece(char c);
// Enum hàng ngang (rank) từ 1-8
enum Rank : u8 {
    Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank_NB
};
char rankToChar(Rank r);
Rank charToRank(char c);

// Enum cột dọc (file) từ A-H  
enum File : u8 {
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, File_NB
};
char fileToChar(File f);
File charToFile(char c);
// Enum ô cờ - định nghĩa tất cả 64 ô trên bàn cờ
// Encoding: A1=0, B1=1, ..., H1=7, A2=8, ..., H8=63
enum Square: int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SquareNone = 0,
    Square_NB = 64
};
// Tạo square từ rank và file
constexpr static Square makeSquare(Rank r, File f) noexcept {
    return Square((r << 3) + f);
}
// Trích xuất rank từ square (chia cho 8)
constexpr static Rank getRank(Square square) noexcept {
    return Rank(square >> 3);
}
// Trích xuất file từ square (lấy 3 bit cuối)
constexpr static File getFile(Square square) noexcept {
    return File(square & 7);
}

// Overload cho int parameter
constexpr static Rank getRank(int square) noexcept {
    return Rank(square >> 3);
}
constexpr static File getFile(int square) noexcept {
    return File(square & 7);
}

// Lật ô cờ theo chiều dọc (A1 <-> A8, B1 <-> B8, ...)
constexpr static inline int flip(int square) noexcept {
    return square ^ 56;
}

// Enum hướng di chuyển cho các quân cờ
enum Direction : int {
    North = 8,        // Lên trên (tăng rank)
    East  = 1,        // Sang phải (tăng file)
    South = -North,   // Xuống dưới (giảm rank)
    West  = -East,    // Sang trái (giảm file)

    NorthEast = North + East,  // Đường chéo phải trên
    SouthEast = South + East,  // Đường chéo phải dưới
    SouthWest = South + West,  // Đường chéo trái dưới
    NorthWest = North + West   // Đường chéo trái trên
};

// Giá trị các quân cờ (centipawn)
enum Value {
    Value_Pawn = 100,    // Tốt = 1 pawn
    Value_Bishop = 330,  // Tượng = 3.3 pawn
    Value_Knight = 320,  // Mã = 3.2 pawn  
    Value_Rook = 500,    // Xe = 5 pawn
    Value_Queen = 900,   // Hậu = 9 pawn
//    Value_King = 32000,  // Vua = vô giá
};

// Bitboard constants cho edge detection và boundary checking
enum : u64 {
    B_D  = 0x00000000000000ffULL,  // Bottom edge (rank 1)
    B_U  = 0xff00000000000000ULL,  // Top edge (rank 8) 
    B_L  = 0x0101010101010101ULL,  // Left edge (file A)
    B_R  = 0x8080808080808080ULL,  // Right edge (file H)

    B_D2 = 0x000000000000ffffULL,  // Bottom 2 ranks
    B_U2 = 0xffff000000000000ULL,  // Top 2 ranks
    B_L2 = 0x0303030303030303ULL,  // Left 2 files
    B_R2 = 0xC0C0C0C0C0C0C0C0ULL,  // Right 2 files
};

// Lấy bit thấp nhất được set (Least Significant Bit)
constexpr u64 lsbBB (u64 input) noexcept  { return input & (-i64(input)); }

// Alias cho bitboard
using BB = u64;


std::string bbToString(BB bb);

// Bitboard constants cơ bản
constexpr BB Rank1_BB = 0xffULL;                    // Rank 1
constexpr BB FileA_BB = 0x0101010101010101ULL;      // File A
constexpr BB LeftDiag_BB = 0x8040201008040201ULL;   // Đường chéo A1-H8
constexpr BB RightDiag_BB = 0x102040810204080ULL;   // Đường chéo H1-A8

// Tạo bitboard cho rank cụ thể
constexpr static BB rankBB(Rank r) noexcept {
    return Rank1_BB << (r << 3);
}
template <Rank r>
constexpr static BB rankBB() noexcept {
    return Rank1_BB << (r << 3);
}

// Tạo bitboard cho file cụ thể
constexpr static BB fileBB(File f) noexcept {
    return FileA_BB << f;
}

// Tính offset cho đường chéo trái (A1-H8 direction)
constexpr static inline int leftDiagOffset(int index) noexcept {
    return getFile(index) - getRank(index);
}
// Tính offset cho đường chéo phải (H1-A8 direction)  
constexpr static inline int rightDiagOffset(int index) noexcept {
    return 7 - getFile(index) - getRank(index);
}
// Lấy bitboard đường chéo trái đi qua ô index
constexpr static inline BB getLeftDiag(int index) noexcept {
    int offset = leftDiagOffset(index) << 3;
    if (offset > 0){
        return LeftDiag_BB >> offset;
    } else {
        return LeftDiag_BB << -offset;
    }
}
// Lấy bitboard đường chéo phải đi qua ô index
constexpr static inline BB getRightDiag(int index) noexcept {
    int offset = rightDiagOffset(index) << 3;
    if (offset > 0){
        return RightDiag_BB >> offset;
    } else {
        return RightDiag_BB << -offset;
    }
}
// Template để lấy đường chéo theo màu (White: right diag, Black: left diag)
template <Color color>
constexpr static BB getSideDiag(int index) noexcept {
    if constexpr (color == White) {
        return getRightDiag(index);
    } else {
        return getLeftDiag(index);
    }
}

// Bitboard chứa tất cả các ô
constexpr BB All_BB = 0xffffffffffffffffULL;


// Template function để shift bitboard theo hướng cụ thể
template<Direction D>
constexpr BB shift(BB b) {
    return D == North         ? b << 8                        // Lên 1 rank
         : D == South         ? b >> 8                        // Xuống 1 rank  
         : D == North + North ? b << 16                       // Lên 2 rank
         : D == South + South ? b >> 16                       // Xuống 2 rank
         : D == East          ? (b & ~fileBB(FileH)) << 1     // Sang phải (không wrap)
         : D == West          ? (b & ~fileBB(FileA)) >> 1     // Sang trái (không wrap)
         : D == NorthEast    ? (b & ~fileBB(FileH)) << 9     // Chéo phải trên
         : D == NorthWest    ? (b & ~fileBB(FileA)) << 7     // Chéo trái trên
         : D == SouthEast    ? (b & ~fileBB(FileH)) >> 7     // Chéo phải dưới
         : D == SouthWest    ? (b & ~fileBB(FileA)) >> 9     // Chéo trái dưới
                              : 0;
}

// Chuyển đổi square index thành bitboard
constexpr static BB indexToBB(Square sq) noexcept {
    return 1ULL << sq;
}
constexpr BB indexToBB(int idx) noexcept {
    return 1ULL << idx;
}
template <int index>
constexpr static BB toBB() noexcept { return 1ULL << index; }

// Tìm vị trí bit thấp nhất được set (Least Significant Bit)
static inline Square lsb(u64 bb) noexcept {
#ifdef __linux__
    return (Square) __builtin_ctzll(bb);
#else
    unsigned long idx;
    _BitScanForward64(&idx, bb);
    return (Square) idx;
#endif
}
static inline int lsbIndex(u64 bb) noexcept {
#ifdef __linux__
    return __builtin_ctzll(bb);
#else
    unsigned long idx;
    _BitScanForward64(&idx, bb);
    return idx;
#endif
}
// Đếm số bit được set trong bitboard
inline int popCount(u64 bb) noexcept {
#ifdef __linux__
    return __builtin_popcountll(bb);
#else
    return __popcnt64(bb);
#endif
}
// Utility functions cho bitboard manipulation
constexpr inline u64 setAllBit(bool comp) noexcept {
    return static_cast<u64>(-comp);
}
constexpr inline u32 setAllBit32(bool comp) noexcept {
    return static_cast<u32>(-comp);
}

// Tìm và xóa bit thấp nhất trong bitboard non-zero
inline Square popLsb(BB& b) noexcept {
    const Square s = lsb(b);
    b &= b - 1;  // Xóa bit thấp nhất
    return s;
}

// Kiểm tra xem bitboard có nhiều hơn 1 bit được set không
constexpr inline bool isMoreThanOne(BB b) noexcept {
    return b & (b - 1);
}

// Enum quyền nhập thành (castling rights)
enum CastlingRights : int {
    NoCastling = 0,
    CastlingWK = 1,     // White King side (O-O)
    CastlingWQ = 2,     // White Queen side (O-O-O)
    CastlingBK = 4,     // Black King side (O-O)
    CastlingBQ = 8,     // Black Queen side (O-O-O)
    CastlingWhite = CastlingWK | CastlingWQ,
    CastlingBlack = CastlingBK | CastlingBQ,
    CastlingRightsAny = CastlingWhite | CastlingBlack,
    CastlingRights_NB = 16
};

// Template để lấy màu từ castling right
template <CastlingRights c>
constexpr static inline Color castlingRightToColor() noexcept {
    if constexpr (c == CastlingWK || c == CastlingWQ){
        return White;
    } else {
        return Black;
    }
}

// Template để lấy vị trí các quân trong nhập thành
template <CastlingRights c, PieceType type, bool isSrc>
constexpr static Square getCastlingIndex() noexcept {
    if constexpr (type == King){
        if constexpr (c == CastlingWK){
            return isSrc ? E1 : G1;  // Vua trắng nhập thành phía vua
        } else if constexpr (c == CastlingWQ){
            return isSrc ? E1 : C1;  // Vua trắng nhập thành phía hậu
        } else if constexpr (c == CastlingBK){
            return isSrc ? E8 : G8;  // Vua đen nhập thành phía vua
        } else if constexpr (c == CastlingBQ){
            return isSrc ? E8 : C8;  // Vua đen nhập thành phía hậu
        }
    } else if constexpr (type == Rook){
        if constexpr (c == CastlingWK){
            return isSrc ? H1 : F1;  // Xe trắng nhập thành phía vua
        } else if constexpr (c == CastlingWQ){
            return isSrc ? A1 : D1;  // Xe trắng nhập thành phía hậu
        } else if constexpr (c == CastlingBK){
            return isSrc ? H8 : F8;  // Xe đen nhập thành phía vua
        } else if constexpr (c == CastlingBQ){
            return isSrc ? A8 : D8;  // Xe đen nhập thành phía hậu
        }
    } else if constexpr (type == Knight){ // Sử dụng để tính & trả về vị trí mà quân xe đi ngang qua quân mã khi nhập thành
        if constexpr (c == CastlingWQ){
            return B1;
        } else if constexpr (c == CastlingBQ){
            return B8;
        }
    }
}

// Tạo bitboard toggle cho nhập thành (source và destination squares)
template<CastlingRights c, PieceType type>
constexpr static BB getCastlingToggle() noexcept {
    return toBB<getCastlingIndex<c, type, true>()>() | toBB<getCastlingIndex<c, type, false>()>();
}

// Đường đi của vua trong nhập thành (bao gồm cả ô xuất phát, đích và ô xe đến)
template<CastlingRights c>
constexpr static BB castlingKingPath() noexcept {
    return toBB<getCastlingIndex<c, King, true>()>() 
        | toBB<getCastlingIndex<c, Rook, false>()>()
        | toBB<getCastlingIndex<c, King, false>()>();
}
// Không gian cần trống cho nhập thành
template<CastlingRights c>
constexpr static BB castlingSpace() noexcept {
    if constexpr (c == CastlingWK || c == CastlingBK){
        return toBB<getCastlingIndex<c, King, false>()>()
            | toBB<getCastlingIndex<c, Rook, false>()>();
    } else {
        return toBB<getCastlingIndex<c, King, false>()>()
            | toBB<getCastlingIndex<c, Rook, false>()>()
            | toBB<getCastlingIndex<c, Knight, false>()>();
    }
}

// Struct đại diện cho một nước đi cờ vua
struct Move {
    enum {
        NullMove = 0,
        FromToBit = 6, SpecialBit = 12,  // Bit positions trong encoding
    };
    // Các loại nước đi (flags)
    enum Flag : u16 {
        Quiet           = (0 << SpecialBit),   // Nước đi thường
        PawnPush2       = (1 << SpecialBit),   // Tốt đi 2 ô
        KingCastling    = (2 << SpecialBit),   // Nhập thành phía vua
        QueenCastling   = (3 << SpecialBit),   // Nhập thành phía hậu
        Capture         = (4 << SpecialBit),   // Bắt quân
        Enpassant       = (5 << SpecialBit),   // Bắt tốt qua đường
        Promotion       = (8 << SpecialBit),   // Phong cấp
        CapturePromotion= Capture + Promotion, // Bắt quân + phong cấp
        All             = (15 << SpecialBit),  // Mask cho tất cả flags
    };
    
    // Constructors
    constexpr inline Move() : move(NullMove), val(0){}

    constexpr inline Move(int src, int dst, Flag flag = Quiet)
        : move(flag + src + (dst << FromToBit)), val(0){}

    constexpr inline Move(int src, int dst, Flag flag, PieceType p)
        : move(flag + src + (dst << FromToBit) + ((p - 2) << 12)), val(0){}

    // Lấy flag của nước đi
    constexpr inline u16 flag() const noexcept {
        return move & All;
    }

    // Kiểm tra loại nước đi
    template <Flag f>
    constexpr inline bool is() const noexcept {
        if constexpr (f == Capture || f == Promotion){
            return f & flag(); // Có nhiều loại capture & promotion
        } else {
            return f == flag();
        }
    }

    // Getter methods
    constexpr inline int src() const noexcept { return move & 0x3f; }            // Ô xuất phát
    constexpr inline int dst() const noexcept { return (move >> 6) & 0x3f; }     // Ô đích
    constexpr inline PieceType getPromotionPieceType() const noexcept { return PieceType(((move >> 12) & 0x03) + 2); }

    std::string getDescription() const noexcept ;
private:
    u16 move;  // Encoding: src(6 bits) + dst(6 bits) + flag(4 bits)
public:
    i16 val;   // Giá trị đánh giá cho move ordering
};

// Enum điểm số đặc biệt
enum Score {
    ScoreMate = 32000,  // Điểm chiếu bí
};

} // namespace kc
















