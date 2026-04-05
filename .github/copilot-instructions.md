Luôn trả lời tôi bằng tiếng Việt
Khi yêu cầu lập kế hoạch, luôn tạo trong đường dẫn plans/

# MyChess - Dự án Cờ Vua C++ Qt

## Tổng quan dự án
- **Ngôn ngữ**: C++ với Qt Framework (v5.12.12)
- **Kiến trúc**: Ứng dụng desktop với chess engine tích hợp
- **Compiler**: MSVC 2017/2019 (Windows)
- **Hệ thống build**: qmake (file .pro)

## Cấu trúc code chính

### 1. Chess Engine (algorithm/)
- **MoveGen**: Sinh nước đi hợp lệ cho tất cả quân cờ
  - Sinh nước đi dựa trên Bitboard
  - Phát hiện ghim quân & xử lý chiếu
  - Template specialization cho quân Trắng/Đen
  - Nước đi đặc biệt: bắt tốt qua đường, nhập thành, phong cấp
- **Evaluation**: Đánh giá vị thế bàn cờ
  - Giá trị quân cờ & bảng giá trị theo vị trí
  - Đánh giá trung cuộc/tàn cuộc với tapered eval
  - Điểm thưởng tính di động
- **Engine**: Tìm kiếm AI với thuật toán negamax
  - Cắt tỉa alpha-beta
  - Sắp xếp nước đi (ưu tiên ăn quân)
  - Tìm kiếm độ sâu thay đổi
- **Board**: Quản lý trạng thái bàn cờ
  - Biểu diễn Bitboard
  - Thực hiện/hoàn tác nước đi
  - Quản lý trạng thái (quyền nhập thành, bắt tốt qua đường)

### 2. Thành phần giao diện (GUI)
- **ChessBoardView**: Widget tùy chỉnh hiển thị bàn cờ
- **MainWindow**: Giao diện chính với các điều khiển
- **PromotionSelectionDialog**: Dialog chọn quân phong cấp

### 3. Tiện ích
- **Logger**: Hệ thống ghi log
- **TreeGraph**: Công cụ trực quan hóa cây tìm kiếm
- **Attack**: Bảng tấn công tính trước

## Quy ước & Patterns
- Template metaprogramming cho code xử lý chung 2 màu quân
- Phép toán Bitboard cho hiệu suất cao
- RAII pattern cho quản lý tài nguyên
- Tổ chức theo namespace (kc::)
- Hungarian notation cho một số biến

## Build & Phát triển
- Cấu hình Debug/Release
- Build song song với -j4
- Qt Designer cho file giao diện (.ui)
- Hệ thống tài nguyên cho hình ảnh (.qrc)
- Script build tùy chỉnh (build.bat/clean.bat)
