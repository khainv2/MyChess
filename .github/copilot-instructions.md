Luôn trả lời tôi bằng tiếng việt
Khi yêu cầu lập kế hoạch, luôn tạo trong đường dẫn plans/

# MyChess - Dự án Cờ Vua C++ Qt

## Tổng quan dự án
- **Ngôn ngữ**: C++ với Qt Framework (v5.12.12)
- **Kiến trúc**: Desktop application với chess engine tích hợp
- **Compiler**: MSVC 2017/2019 (Windows)
- **Build system**: qmake (.pro file)

## Cấu trúc code chính

### 1. Chess Engine (algorithm/)
- **MoveGen**: Sinh nước đi hợp lệ cho tất cả quân cờ
  - Bitboard-based move generation
  - Pin detection & check handling  
  - Template specialization cho White/Black
  - Special moves: en passant, castling, promotion
- **Evaluation**: Đánh giá vị thế bàn cờ
  - Material values & piece-square tables
  - Middlegame/Endgame evaluation với tapered eval
  - Mobility bonus
- **Engine**: AI search với negamax algorithm
  - Alpha-beta pruning
  - Move ordering (captures first)
  - Variable depth search
- **Board**: Quản lý trạng thái bàn cờ
  - Bitboard representation
  - Move making/unmaking
  - State management (castling rights, en passant)

### 2. GUI Components  
- **ChessBoardView**: Custom widget hiển thị bàn cờ
- **MainWindow**: UI chính với controls
- **PromotionSelectionDialog**: Dialog chọn quân phong cấp

### 3. Utilities
- **Logger**: Logging system
- **TreeGraph**: Visualization tool
- **Attack**: Precomputed attack tables

## Patterns & Conventions
- Template metaprogramming cho color-generic code
- Bitboard operations cho hiệu suất cao
- RAII pattern cho resource management
- Namespace organization (kc::)
- Hungarian notation cho một số biến

## Build & Development
- Debug/Release configurations
- Parallel builds với -j4
- Qt Designer cho UI files
- Resource system cho images
- Custom build scripts (build.bat/clean.bat)
