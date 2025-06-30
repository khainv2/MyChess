# MyChess - Ứng dụng Cờ Vua

## Giới thiệu
MyChess là một ứng dụng cờ vua được phát triển bằng Qt C++, bao gồm engine cờ vua và giao diện đồ họa.

## Build Instructions

### Cách build nhanh (Khuyến nghị)
1. Đảm bảo Qt 5.12.12 đã được cài đặt tại `C:\Qt\Qt5.12.12`
2. Double-click file `build.bat` 
3. Chọn loại build (Debug/Release) 
4. Chờ build hoàn thành

### Build thủ công
Xem hướng dẫn chi tiết trong file `plans/build-guide.md`

## Scripts có sẵn

- `build.bat` - Script build tự động
- `clean.bat` - Script xóa files build
- `plans/build-guide.md` - Hướng dẫn build chi tiết

## Cấu trúc dự án

```
MyChess/
├── _build/              # Files build (tạo tự động)
├── algorithm/           # Engine cờ vua
├── res/                 # Resources (hình ảnh)
├── plans/               # Tài liệu kế hoạch
├── *.cpp, *.h          # Source code giao diện
├── *.ui                # Qt Designer files
├── build.bat           # Script build
├── clean.bat           # Script clean
└── MyChess.pro         # Qt project file
```

## Yêu cầu hệ thống

- **Qt**: 5.12.12
- **OS**: Windows  
- **Compiler**: MSVC 2017/2019 hoặc MinGW
- **RAM**: Tối thiểu 4GB

## Sử dụng

1. Build dự án bằng `build.bat`
2. Chạy file `MyChess.exe` trong thư mục `_build/release/` hoặc `_build/debug/`
3. Bắt đầu chơi cờ!

## Liên hệ

Nếu gặp vấn đề trong quá trình build, vui lòng kiểm tra:
1. Đường dẫn Qt installation
2. Compiler compatibility  
3. Xem log lỗi trong terminal
