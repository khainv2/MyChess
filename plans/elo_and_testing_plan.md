# Kế hoạch Tăng cường Elo và Phương pháp Thử nghiệm

## Mục tiêu
- Nâng cấp engine từ ~1400-1600 ELO lên 1800-2000 ELO.

## Phần 1: Công nghệ & Kỹ thuật cải thiện
1. Transposition Table (Hash Table) để cache kết quả tìm kiếm.
2. Quiescence Search xử lý hiệu ứng chân trời (horizon effect).
3. Iterative Deepening kết hợp quản lý thời gian (time management).
4. Killer Move & History Heuristics cải thiện thứ tự nước đi.
5. Null Move Pruning & Late Move Reductions giảm nút không quan trọng.
6. Aspiration Windows & Principal Variation Search (PVS) tối ưu alpha-beta.
7. Mở rộng hàm đánh giá (evaluation):
   - King Safety
   - Pawn Structure (passed, isolated, doubled pawns)
   - Piece Coordination & Attack Zones

## Phần 2: Phương pháp thử nghiệm
1. **Perft Tests**: xác minh tính đầy đủ và hợp lệ của move generator.
2. **Unit Tests** cho:
   - Board state (make/unmake move)
   - Evaluation function (giá trị static)
   - Search algorithm (negamax, alpha-beta)
3. **Self-play Batch**: engine tự đấu 1.000+ ván, thu số liệu thắng/hòa/thua.
4. **Engine vs Engine**: sử dụng `cutechess-cli` để thi đấu với các engine chuẩn trên nhiều time control khác nhau.
5. **EPD Suite Tests**:
   - Tactical test suites (traps, tactics)
   - Strategic test suites (endgame, pawn structure)
6. **Regression Tests**: lưu tập EPD với đáp án kỳ vọng, đảm bảo không regress khi chỉnh sửa.
7. **Continuous Integration**:
   - Tự động chạy perft, unit tests, self-play trên mỗi commit.
   - Báo cáo kết quả qua GitHub Actions hoặc build server.

## Phần 3: Công cụ & Framework
- cutechess-cli
- Fishtest (hoặc custom test harness)
- EPD suites (CTC, LCT, WAC)
- CCRL/CEGT benchmarks
- Công cụ phân tích PGN & tính toán Elo

## Phần 4: Lộ trình thực hiện
| Phase | Nội dung chính | Thời gian dự kiến |
|-------|----------------|-------------------|
| 1     | Transposition Table, Quiescence Search, Iterative Deepening + perft/unit tests | 2 tuần |
| 2     | Killer/History Heuristics, Null Move, Late Move Reductions + self-play batch | 2 tuần |
| 3     | Aspiration/PVS, nâng cấp evaluation + EPD Suite Tests | 3 tuần |
| 4     | Thiết lập CI/CD, Engine vs Engine Tournament, tổng hợp báo cáo Elo | 1 tuần |

*Lưu ý*: Không cần phải chạy thử mọi thay đổi bằng game trực tiếp với máy; kết hợp perft, unit tests và self-play/simulation sẽ tiết kiệm thời gian và đảm bảo độ ổn định trước khi so tài thực chiến.
