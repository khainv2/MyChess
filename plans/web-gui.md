# Web GUI cho MyChess

## Mục tiêu
Web GUI HTML/JS, host trên mạng nội bộ. Người chơi vs engine là chính. Có hiển thị info, lịch sử, undo, FEN I/O, engine vs engine.

## Kiến trúc

```
Browser (HTML/JS)  <— WebSocket —>  Node.js server  <— stdin/stdout —>  MyChessUCI
```

- Mỗi tab/client = 1 session = 1 engine process riêng.
- Server lắng nghe `0.0.0.0:<port>` cho LAN.

## Cấu trúc thư mục mới

```
web/
  package.json
  server.js                # Express + ws + EngineProcess class
  README.md                # Cách chạy
  public/
    index.html
    style.css
    app.js                 # State + UI controller
    chess.js               # Vendored chess.js (luật cờ phía client)
    pieces/                # SVG quân (Wikipedia/lichess - public domain)
```

## Engine binary

Vấn đề: `_build_uci_linux/MyChessUCI` cần libQt5Core.so.5 (không cài trên máy hiện tại).
`uci_main.cpp` thực ra không dùng Qt — chỉ có `QCoreApplication app(argc, argv);` đứng yên.

**Giải pháp**: Tạo `MyChessUCI_noqt.pro` (hoặc Makefile thuần) bỏ Qt, rebuild → binary standalone. Đặt cấu hình đường dẫn binary trong `web/server.js` qua env var `MYCHESS_UCI_PATH`.

## Giao thức WebSocket (JSON)

**Client → Server**
- `{type:"newgame", fen?:string}` — reset, gửi `ucinewgame` + `position`.
- `{type:"move", uci:string}` — append vào lịch sử nội bộ session, gửi `position ... moves ...`.
- `{type:"go", movetimeMs?:number, depth?:number}` — gửi `go ...`.
- `{type:"stop"}` — gửi `stop`.
- `{type:"undo", count?:number}` — bỏ N nước cuối, resync `position`.

**Server → Client**
- `{type:"info", depth, score:{cp|mate}, time, nodes, nps, pv:[uci...]}`
- `{type:"bestmove", uci}`
- `{type:"ready"}` / `{type:"error", msg}`

## Frontend

- **Bàn cờ**: 8x8 div grid, kéo-thả quân (HTML5 drag-and-drop). Highlight nước hợp lệ khi click.
- **Luật cờ phía client**: vendored `chess.js` (validate, sinh nước, FEN, PGN, kết thúc ván).
- **Promotion**: dialog modal chọn Q/R/B/N.
- **Sidebar**:
  - Chế độ: Người vs Engine / Engine vs Engine / Hai người.
  - Người chọn bên (trắng/đen), thời gian suy nghĩ engine (vd. slider 100ms–10s).
  - Nút: New game, Undo, Flip board, Copy FEN, Paste FEN, Stop engine.
  - Lịch sử nước đi (SAN) — click để rewind.
  - Info panel: depth/score/nps/pv (live).
- **Engine vs Engine**: Server đơn giản — sau mỗi `bestmove`, client tự apply rồi gửi `go` cho engine bên kia. Logic ở client.

## Phụ thuộc Node.js

- `express` — phục vụ static files.
- `ws` — WebSocket.

## Bước thực hiện

1. Tạo `algorithm/uci_main_noqt.cpp` (copy uci_main.cpp, bỏ include Qt + QCoreApplication) hoặc sửa trực tiếp `uci_main.cpp` (Qt dependency hoàn toàn không cần).
2. Tạo Makefile thuần (g++ -O2 -std=c++17) build engine standalone trong `_build_uci_nodeps/`.
3. Scaffold `web/` (package.json, server.js, public/).
4. Implement `EngineProcess` class (spawn, parse UCI line, queue stop, idle timeout).
5. Implement frontend chess UI + chess.js + WebSocket client.
6. Test end-to-end: 1 client chơi vs engine.
7. Engine vs engine mode.
8. Test trên LAN (browser máy khác).

## Out of scope (giai đoạn này)
- Auth, rate limit, multi-room dùng chung, lưu ván vào DB. Có thể thêm sau.
