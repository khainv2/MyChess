# Hướng dẫn Build Dự án MyChess

## Yêu cầu hệ thống

- **Qt Framework**: Qt 5.12.12 (đã cài đặt tại `C:\Qt\Qt5.12.12`)
- **Compiler**: MinGW hoặc MSVC (tùy thuộc vào cài đặt Qt)
- **CMake**: Không bắt buộc (sử dụng qmake)
- **Hệ điều hành**: Windows

## Cấu trúc thư mục sau khi build

```
MyChess/
├── _build/           # Thư mục chứa tất cả file build
│   ├── debug/        # Build debug
│   ├── release/      # Build release
│   └── Makefile      # File make được tạo
├── plans/            # Thư mục kế hoạch
├── algorithm/        # Source code thuật toán
├── res/              # Resources (hình ảnh, qrc)
└── *.cpp, *.h        # Source code chính
```

## Các bước build thủ công

### 1. Chuẩn bị môi trường

Đảm bảo Qt đã được cài đặt tại `C:\Qt\Qt5.12.12` và compiler được hỗ trợ.

### 2. Tạo thư mục build

```powershell
# Tại thư mục gốc dự án
mkdir _build
cd _build
```

### 3. Tạo Makefile với qmake

```powershell
# Sử dụng qmake để tạo Makefile
C:\Qt\Qt5.12.12\msvc2017_64\bin\qmake.exe ..\MyChess.pro
```

**Lưu ý**: Thay `msvc2017_64` bằng phiên bản compiler tương ứng:
- `msvc2017_64` - Visual Studio 2017 64-bit
- `msvc2019_64` - Visual Studio 2019 64-bit  
- `mingw73_64` - MinGW 64-bit

### 4. Build dự án

#### Build Debug:
```powershell
nmake debug
# Hoặc nếu dùng MinGW:
# mingw32-make debug
```

#### Build Release:
```powershell
nmake release
# Hoặc nếu dùng MinGW:
# mingw32-make release
```

#### Build cả hai:
```powershell
nmake
# Hoặc nếu dùng MinGW:
# mingw32-make
```

### 5. Chạy ứng dụng

File thực thi sẽ được tạo trong:
- **Debug**: `_build\debug\MyChess.exe`
- **Release**: `_build\release\MyChess.exe`

## Lưu ý quan trọng

1. **Thư mục _build**: Tất cả file build sẽ được đặt trong thư mục `_build` để tránh lộn xộn với source code
2. **Dependencies**: Đảm bảo các file Qt DLL cần thiết có thể truy cập được
3. **Resources**: File `main.qrc` chứa resources sẽ được compile tự động
4. **Clean build**: Để clean build, xóa toàn bộ thư mục `_build` và tạo lại

## Troubleshooting

### Lỗi thường gặp:

1. **Không tìm thấy qmake**: Kiểm tra đường dẫn Qt installation
2. **Lỗi compiler**: Đảm bảo compiler tương thích với Qt version
3. **Missing DLL**: Copy các Qt DLL cần thiết vào thư mục output

### Kiểm tra Qt installation:
```powershell
C:\Qt\Qt5.12.12\msvc2017_64\bin\qmake.exe -version
```

## Automation

Sử dụng batch script `build.bat` để tự động hóa quá trình build (xem file `build.bat` trong thư mục gốc).
