# MyChess Web GUI

Web GUI cho engine MyChess. Frontend HTML/JS, backend Node.js làm cầu nối tới `MyChessUCI` qua giao thức UCI.

```
Browser  ⟷  WebSocket  ⟷  Node (server.js)  ⟷  stdin/stdout  ⟷  MyChessUCI
```

Mỗi tab/người chơi = 1 process engine riêng.

## Yêu cầu

- Node.js 16+
- Engine binary: `_build_uci_nodeps/MyChessUCI` (build bằng `make -f Makefile.uci` từ thư mục gốc — không cần Qt).

## Build engine standalone (chỉ làm 1 lần)

```bash
cd ..                    # về thư mục gốc dự án
make -f Makefile.uci
```

Output: `_build_uci_nodeps/MyChessUCI` (không phụ thuộc Qt5).

## Cài & chạy server

```bash
cd web
npm install
npm start
```

Mặc định listen trên `0.0.0.0:8080`. Mở browser: `http://<IP-máy-host>:8080`.

### Chạy nền (host LAN nội bộ)

**Phương án A — systemd** (máy host có systemd PID 1, ví dụ Ubuntu server thông thường):

```bash
sudo cp web/mychess-web.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now mychess-web
sudo systemctl status mychess-web
journalctl -u mychess-web -f       # xem log
```

Sửa `User=`, `WorkingDirectory=`, `MYCHESS_UCI_PATH=` trong `mychess-web.service` cho phù hợp host của bạn.

**Phương án B — nohup script** (máy không có systemd, ví dụ container/WSL không bật systemd):

```bash
./run-bg.sh        # khởi động nền, log tại logs/server.log, PID tại logs/server.pid
./stop-bg.sh       # dừng
tail -f logs/server.log
```

`run-bg.sh` từ chối start nếu instance trước còn chạy. Để chạy lại sau reboot, thêm vào crontab `@reboot /path/to/web/run-bg.sh` (cron cũng cần daemon — nếu không có, gọi tay sau khi máy bật).

### Biến môi trường

| Biến                | Mặc định                              | Ý nghĩa                          |
|---------------------|---------------------------------------|----------------------------------|
| `PORT`              | `8080`                                | Port HTTP/WS                     |
| `HOST`              | `0.0.0.0`                             | Interface bind (LAN dùng `0.0.0.0`) |
| `MYCHESS_UCI_PATH`  | `../_build_uci_nodeps/MyChessUCI`     | Đường dẫn binary engine          |

```bash
PORT=9000 MYCHESS_UCI_PATH=/path/to/MyChessUCI npm start
```

## Tính năng UI

- Người vs Engine, Engine vs Engine, Hai người (cùng máy).
- Chọn bên (trắng/đen) cho người chơi.
- Slider thời gian engine suy nghĩ: 100 ms – 10 s.
- Lịch sử nước đi SAN, click để xem lại vị trí cũ (read-only).
- Undo (PvE: undo cả nước engine + nước người).
- Lật bàn cờ.
- Copy/Load FEN.
- Live info: depth, score (cp/mate), nodes, NPS, PV (đã convert sang SAN).
- Highlight: nước cuối, ô được chọn, các ô đi hợp lệ, vua bị chiếu.
- Promotion dialog.

## Giao thức WebSocket (JSON)

**Client → Server**

| `type`       | Trường khác                          | UCI tương ứng                         |
|--------------|--------------------------------------|---------------------------------------|
| `newgame`    | `fen?: string`                       | `ucinewgame` + `position [...]`       |
| `position`   | `fen?: string`, `moves?: string[]`   | `position [fen X \| startpos] moves …` |
| `go`         | `movetimeMs?: number`, `depth?: number` | `go movetime/depth …`              |
| `stop`       | —                                    | `stop`                                |

**Server → Client**

| `type`       | Trường                                                |
|--------------|-------------------------------------------------------|
| `ready`      | (engine đã `uciok`)                                   |
| `info`       | `depth`, `score:{kind:'cp'\|'mate', value}`, `time`, `nodes`, `nps`, `pv:string[]` |
| `bestmove`   | `uci: string \| null`                                 |
| `error`      | `msg: string`                                         |

## Test

Trong khi server đang chạy:

```bash
node test_ws.js         # smoke test 1 ván nhanh
node test_ws_full.js    # 3 kịch bản (position+moves, stop, EvE chain)
```

## Cấu trúc

```
web/
  server.js              # Express + ws + EngineProcess
  test_ws.js
  test_ws_full.js
  package.json
  public/
    index.html
    style.css
    app.js               # UI controller (ESM, dùng chess.js)
    vendor/chess.js      # luật cờ phía client
    pieces/{w,b}{K,Q,R,B,N,P}.svg   # bộ Cburnett (CC BY-SA 3.0)
```

## Ghi chú & giới hạn

- Không có auth/rate-limit. Chỉ phù hợp mạng nội bộ tin cậy.
- Mỗi kết nối WS = 1 process engine. 50 người chơi = 50 process — kiểm tra RAM/CPU nếu host nhiều.
- Reload trang = mất ván (state lưu phía client). Có thể export FEN trước.
- Promotion mặc định khi UCI thiếu ký tự (vd. `e7e8`) sẽ phong hậu thông qua chess.js (đã được app.js xử lý).
