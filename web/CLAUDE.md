# Web GUI — workflow cho sửa đổi

File này là chỉ dẫn cho Claude Code khi user yêu cầu sửa web GUI. Tự động được load khi làm việc trong `web/`.

## Bố cục dự án 2 lớp

| Phần          | Thư mục                          | Vai trò                                              |
|---------------|----------------------------------|------------------------------------------------------|
| **Engine**    | `algorithm/`, `uci_main.cpp`     | C++ chess engine + UCI protocol. Build độc lập.      |
| **Backend**   | `web/server.js`                  | Node.js bridge: WebSocket ↔ stdin/stdout của engine. |
| **Frontend**  | `web/public/`                    | HTML/CSS/JS thuần (ESM, dùng chess.js).              |

Mỗi lớp sửa độc lập. Khi user nói "sửa web", thường ý họ là frontend (`web/public/`) hoặc backend (`web/server.js`); ít khi đụng engine.

## Trạng thái runtime

- **Cách host**: máy này KHÔNG có systemd PID 1 (PID 1 là `sshd`, môi trường container Ubuntu 24.04). `systemctl` không dùng được — luôn dùng `web/run-bg.sh` / `web/stop-bg.sh`.
- **Process**: PID lưu tại `web/logs/server.pid`. Log tại `web/logs/server.log`.
- **Port**: `8080` mặc định, bind `0.0.0.0` cho LAN.
- **Engine binary**: `_build_uci_nodeps/MyChessUCI` (không phụ thuộc Qt). Mỗi WebSocket connect = 1 process engine spawn.
- **Unit file mẫu**: `web/mychess-web.service` — chỉ dùng khi deploy đến host có systemd thật.

## Lệnh chuẩn khi sửa

### Sửa frontend (HTML/CSS/JS trong `web/public/`)
Không cần restart server — Express serve static từ disk, browser refresh là thấy. Chỉ cần test:
```bash
node /home/dev2/test/MyChess/web/test_ws.js          # smoke 1 ván
```
Sau đó user reload browser kiểm tra UI.

### Sửa backend (`web/server.js`)
Phải restart:
```bash
/home/dev2/test/MyChess/web/stop-bg.sh
/home/dev2/test/MyChess/web/run-bg.sh
tail -20 /home/dev2/test/MyChess/web/logs/server.log
```

### Sửa engine (`algorithm/*.cpp`, `uci_main.cpp`)
Rebuild standalone + restart server (vì process engine được spawn từ binary):
```bash
make -f /home/dev2/test/MyChess/Makefile.uci
/home/dev2/test/MyChess/web/stop-bg.sh
/home/dev2/test/MyChess/web/run-bg.sh
```
Smoke test engine trước khi qua web:
```bash
printf 'uci\nposition startpos\ngo movetime 500\n' | /home/dev2/test/MyChess/_build_uci_nodeps/MyChessUCI | tail -5
```

Lưu ý: build Qt cũ (`MyChessUCI.pro`, `nmake`) không bị ảnh hưởng — `Makefile.uci` chỉ thêm `-Ialgorithm/qt_compat` để cung cấp shim cho `<QDebug>` và `<QElapsedTimer>`.

## Quy ước

- Không thêm phụ thuộc npm mới khi không cần (`express`, `ws` là đủ).
- Không thêm framework JS frontend (React/Vue) — giữ vanilla ES modules.
- Quân cờ SVG ở `public/pieces/` (Cburnett, CC BY-SA 3.0). Đừng đổi sang format khác.
- Không bật auth/HTTPS — server chỉ cho LAN tin cậy. Nếu user muốn expose ra internet thì cần bàn lại.

## Giao thức WebSocket

Đã document ở `web/README.md`. Khi sửa giao thức, sửa đồng thời:
- `web/server.js` (parse/translate)
- `web/public/app.js` (gửi/nhận)
- `web/README.md` (bảng giao thức)
- `web/test_ws_full.js` (test scenarios)

## Test trước khi báo "xong"

- `node web/test_ws.js` — bestmove cơ bản
- `node web/test_ws_full.js` — position+moves, stop, EvE chain
- **UI**: tôi không có browser headless, sửa frontend xong phải báo user tự reload kiểm tra.
