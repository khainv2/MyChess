# Phân Tích So Sánh Stockfish vs MyChess - Khuyến Nghị Cải Thiện

## Tổng Quan

Sau khi nghiên cứu kỹ source code Stockfish (phiên bản mới nhất, ~/Stockfish/src/) và so sánh với MyChess engine, dưới đây là báo cáo chi tiết các kỹ thuật có thể áp dụng, xếp theo mức độ ưu tiên và khả năng thực hiện.

---

## 1. TRANSPOSITION TABLE - Cải thiện đáng kể

### Hiện tại MyChess (`tt.h/cpp`):
- TTEntry lưu full 64-bit key (8 bytes lãng phí)
- Single-entry per bucket (collision = mất dữ liệu)
- Replacement policy đơn giản: `depth >= old_depth`
- Không có age/generation tracking
- `key % numEntries` để hash (chậm, phân bố không đều)

### Stockfish (`tt.h/cpp`):
- TTEntry chỉ 10 bytes: `key16 (2B) + depth8 (1B) + genBound8 (1B) + move16 (2B) + value16 (2B) + eval16 (2B)`
- **Cluster = 3 entries + 2 bytes padding = 32 bytes** (vừa 1 cache line)
- Replacement dùng `depth - 8 * relative_age`: ưu tiên giữ entry sâu VÀ mới
- Generation tracking qua 5 bits trong `genBound8`
- Hash function: `mul_hi64(key, clusterCount)` - nhanh hơn modulo

### Khuyến nghị:
```
Ưu tiên: CAO | Độ khó: TRUNG BÌNH | ELO ước tính: +50-100
```

1. **Chuyển sang cluster-based TT (3 entries/cluster, 32 bytes)**
   - Giảm collision rate đáng kể
   - Cache-friendly vì 32 bytes = half cache line

2. **Lưu chỉ 16-bit key** thay vì 64-bit
   - Tiết kiệm 6 bytes/entry → nhiều entries hơn trong cùng RAM

3. **Thêm generation counter** (`tt.new_search()` mỗi root search)
   - Replacement ưu tiên xóa entries cũ
   - Formula: `replace_value = depth8 - relative_age(generation8)`

4. **Dùng `mul_hi64` thay vì modulo** để tính index
   ```cpp
   // Stockfish style
   TTEntry* first_entry(Key key) const {
       return &table[mul_hi64(key, clusterCount)].entry[0];
   }
   ```

5. **Lưu static eval trong TT** (như Stockfish lưu `eval16`)
   - Tránh tính lại eval khi TT hit

---

## 2. SEARCH - Nhiều kỹ thuật thiếu hoặc đơn giản

### 2A. Late Move Reductions (LMR) - Cải thiện mạnh

**MyChess hiện tại** (`engine.cpp:320-330`):
```cpp
// LMR đơn giản: chỉ reduce 1 ply cho quiet moves sau move thứ 4
if (i >= LMR_FULL_MOVES && depth >= LMR_MIN_DEPTH && !inCheck
    && !move.is<Move::Capture>() && !move.is<Move::Promotion>())
    newDepth -= 1;
```

**Stockfish** (`search.cpp:619-620, 1247-1278`):
```cpp
// Reduction table: logarithmic
reductions[i] = int(2763 / 128.0 * std::log(i));

// Actual LMR: r phụ thuộc ~20 yếu tố
int r = reduction(improving, depth, moveCount, delta);
r += 1013;  // ttPv bonus
r -= 2819 + PvNode * 973 + ...;  // PvNode reduction
r += 3611 + 985 * !ttData.move;  // cutNode increase
r -= ss->statScore * 428 / 4096; // history-based
// ... và nhiều nữa
Depth d = std::max(1, std::min(newDepth - r / 1024, newDepth + 2));
```

**Khuyến nghị:**
```
Ưu tiên: RẤT CAO | Độ khó: TRUNG BÌNH | ELO ước tính: +100-200
```

1. **Dùng logarithmic reduction table:**
   ```cpp
   int reductions[MAX_MOVES];
   for (int i = 1; i < MAX_MOVES; i++)
       reductions[i] = int(21.0 * std::log(i));  // Simplified
   
   int r = reductions[depth] * reductions[moveCount];
   ```

2. **Điều chỉnh reduction theo:**
   - History score (quiet moves hay gây cutoff → reduce ít hơn)
   - PV node (reduce ít hơn)
   - Cut node (reduce nhiều hơn)
   - Improving flag (eval đang tốt lên → reduce ít hơn)

3. **Re-search logic sau LMR:**
   - Nếu reduced search > alpha → full depth re-search
   - Stockfish còn có deeper/shallower re-search logic

### 2B. Null Move Pruning - Cải thiện

**MyChess** (`engine.cpp:248-262`):
```cpp
// Fixed R = 2, single check
depth - 1 - NULL_MOVE_R  // Always reduce by 2
```

**Stockfish** (`search.cpp:910-942`):
```cpp
// Dynamic R based on depth
Depth R = 7 + depth / 3;
// Verification search at high depths
if (nmpMinPly || depth < 16)
    return nullValue;
// Full verification search
nmpMinPly = ss->ply + 3 * (depth - R) / 4;
Value v = search<NonPV>(pos, ss, beta - 1, beta, depth - R, false);
```

**Khuyến nghị:**
```
Ưu tiên: CAO | Độ khó: THẤP | ELO ước tính: +30-50
```

1. **Dynamic R:** `R = 3 + depth / 3` (tăng dần theo depth)
2. **Verification search** ở depth cao (>= 12-14) để tránh zugzwang

### 2C. Razoring - Mới hoàn toàn

**MyChess:** Không có.

**Stockfish** (`search.cpp:890-891`):
```cpp
if (!PvNode && eval < alpha - 502 - 306 * depth * depth)
    return qsearch<NonPV>(pos, ss, alpha, beta);
```

**Khuyến nghị:**
```
Ưu tiên: TRUNG BÌNH | Độ khó: THẤP | ELO ước tính: +10-20
```
Thêm razoring đơn giản:
```cpp
if (!isRoot && !inCheck && depth <= 3
    && eval + 300 * depth < alpha)
    return quiescence<color>(board, alpha, beta);
```

### 2D. Futility Pruning - Mới hoàn toàn

**MyChess:** Không có (đã bỏ aspiration window nhưng chưa có futility).

**Stockfish** (`search.cpp:893-907`): Futility pruning phức tạp với margin phụ thuộc depth, improving, correction history.

**Khuyến nghị:**
```
Ưu tiên: CAO | Độ khó: THẤP | ELO ước tính: +30-50
```
```cpp
// Trước move loop, sau static eval
if (!isRoot && !inCheck && depth <= 6) {
    int futilityMargin = 100 * depth;
    if (eval - futilityMargin >= beta)
        return eval;
}

// Trong move loop, cho quiet moves:
if (!inCheck && depth <= 6 && !move.is<Move::Capture>()) {
    int futilityValue = eval + 100 + 80 * depth;
    if (futilityValue <= alpha)
        continue;  // Skip move
}
```

### 2E. Singular Extensions - Nâng cao

**MyChess:** Không có extensions.

**Stockfish** (`search.cpp:1146-1198`): Singular extension khi TT move vượt trội → extend 1-3 ply. Multi-cut pruning khi nhiều moves fail high.

**Khuyến nghị:**
```
Ưu tiên: TRUNG BÌNH | Độ khó: CAO | ELO ước tính: +30-50
```
Implement singular extension đơn giản:
- Nếu TT move có `BOUND_LOWER` và depth >= 6
- Search lại KHÔNG có TT move với `singularBeta = ttValue - 2 * depth`
- Nếu tất cả moves khác fail low → extend TT move thêm 1 ply

### 2F. Check Extensions

**MyChess:** Extend khi in check (`if (depth <= 0 && !inCheck)` ở line 219 thực chất là check extension).

**Stockfish:** Không extend riêng cho checks, dùng singular extensions thay thế.

→ MyChess đã có, OK.

---

## 3. MOVE ORDERING - Cải thiện mạnh

### Hiện tại MyChess (`engine.cpp:273-295`):
1. TT move (30000)
2. Captures: MVV-LVA (10000+)
3. Killer moves (9000, 8999)
4. History heuristic `history[piece][toSquare]`

### Stockfish (`movepick.cpp`):
1. TT move
2. **Good captures**: MVV + Capture History + SEE filtering
3. **Good quiets**: Butterfly History + Pawn History + 6 Continuation Histories + Low Ply History + threat-based scoring
4. **Bad captures** (SEE < 0)
5. **Bad quiets**

### Khuyến nghị:
```
Ưu tiên: RẤT CAO | Độ khó: TRUNG BÌNH | ELO ước tính: +50-100
```

1. **Capture History** `captureHistory[piece][toSq][capturedPieceType]`
   - Update khi capture gây cutoff: `captureHistory[pc][to][captured] += depth * depth`
   - Scoring: `MVV * 7 + captureHistory`

2. **Countermove Heuristic** (ngoài Stockfish nhưng phổ biến):
   - `countermoves[piece][toSq]` = nước gây cutoff sau nước đối phương
   - Dùng như killer thứ 3

3. **SEE pruning trong move ordering:**
   - Captures có SEE < 0 → đẩy xuống sau quiet moves
   - MyChess đã có hàm `see()`, chỉ cần dùng nó

4. **Continuation History** (1 level là đủ):
   - `contHist[prevPiece][prevTo][piece][to]`
   - Biết nước đi nào hay tốt sau nước trước đó

5. **History penalty (history malus):**
   - Khi một quiet move gây cutoff → GIẢM history của tất cả quiet moves đã search trước đó
   ```cpp
   // Khi bestMove gây beta cutoff:
   for (int j = 0; j < i; j++) {
       if (!searchedMoves[j].is<Move::Capture>())
           history[pc][dst] -= depth * depth;  // malus
   }
   ```

---

## 4. ASPIRATION WINDOWS - Cải thiện nhỏ

### MyChess (`engine.cpp:154-197`):
- Fixed delta = 50 centipawns
- Mở rộng gấp đôi khi fail

### Stockfish (`search.cpp:356-422`):
- Dynamic delta = `5 + threadIdx % 8 + abs(meanSquaredScore) / 10208`
- Center quanh `averageScore` (not previous score)
- Mở rộng `delta += delta / 3` (33%, không phải 100%)
- Adjust search depth khi fail high

**Khuyến nghị:**
```
Ưu tiên: THẤP | Độ khó: THẤP | ELO ước tính: +5-10
```
- Giảm initial window: 50 → 25-30
- Mở rộng 50% thay vì 100%: `delta = delta * 3 / 2`
- Track `averageScore` thay vì chỉ `bestValue`

---

## 5. EVALUATION - Cải thiện trung bình

### MyChess hiện tại:
- Tapered eval (MG/EG)
- PST từ PeSTO/Stockfish cũ
- Mobility (đếm moves, tốn performance)
- Passed pawns, Bishop pair, King safety

### Stockfish: Dùng NNUE (neural network), KHÔNG dùng classical eval nữa.

### Khuyến nghị (không NNUE):
```
Ưu tiên: TRUNG BÌNH | Độ khó: TRUNG BÌNH-CAO | ELO ước tính: +30-80
```

1. **Bỏ mobility từ `estimate()`** — quá tốn thời gian
   - `MoveGen::instance->countMoveList` sinh tất cả moves chỉ để đếm
   - Thay bằng pseudo-mobility: đếm attacked squares từ attack tables (không cần generate moves)
   ```cpp
   // Pseudo-mobility cho knight:
   int knightMobility = popCount(knightAttacks & ~ourPieces);
   ```

2. **Rook on open/semi-open file:**
   ```cpp
   BB fileMask = fileBB(file_of(rookSq));
   if (!(fileMask & ourPawns)) bonus += (fileMask & theirPawns) ? 10 : 20;
   ```

3. **Pawn structure mở rộng:**
   - Isolated pawns: penalty khi không có tốt cùng file liền kề
   - Doubled pawns: penalty khi 2+ tốt cùng file
   - Protected passed pawns: bonus thêm nếu passed pawn được bảo vệ

4. **King pawn shield:**
   - Bonus khi có 2-3 tốt trước vua (rank 2-3 trước vua)
   - Penalty khi mất tốt shield

5. **Tối ưu quiescence eval:**
   - `estimateFast()` loop qua 64 ô → dùng bitboard iterate thay vì
   ```cpp
   BB occ = board.getOccupancy();
   while (occ) {
       Square sq = popLsb(occ);
       Piece pc = board.pieces[sq];
       mg[color_of(pc)] += MG_Table[pc][sq];
       ...
   }
   ```

---

## 6. QUIESCENCE SEARCH - Cải thiện

### MyChess (`engine.cpp:397-485`):
- Stand-pat
- Delta pruning (standPat + captureValue + 200 < alpha → skip)
- MVV-LVA ordering (std::sort - tốn)
- MaxQDepth = 32

### Stockfish:
- TT probe trong qsearch
- Futility pruning dùng `futilityBase + PieceValue`
- SEE pruning (chỉ search captures có SEE >= 0)
- Check evasions riêng

**Khuyến nghị:**
```
Ưu tiên: CAO | Độ khó: TRUNG BÌNH | ELO ước tính: +20-40
```

1. **TT probe trong qsearch** — rất quan trọng
   ```cpp
   if (tt.probe(hash, ttEntry)) {
       if (ttEntry.flag == TT_EXACT) return ttEntry.score;
       if (ttEntry.flag == TT_LOWER_BOUND && ttEntry.score >= beta) return ttEntry.score;
       if (ttEntry.flag == TT_UPPER_BOUND && ttEntry.score <= alpha) return ttEntry.score;
   }
   ```

2. **SEE pruning** thay delta pruning:
   ```cpp
   if (see(board, move) < 0) continue;  // Skip losing captures
   ```

3. **Dùng pick-best thay std::sort** trong qsearch (đã dùng sort ở line 456)

---

## 7. TIME MANAGEMENT - Thiếu hoàn toàn

### MyChess: Fixed depth = 9, không có time control.

### Stockfish (`timeman.h/cpp`, `search.cpp:498-541`):
- `optimum()` và `maximum()` time allocation
- Dynamic time based on: falling eval, best move stability, node effort
- Can stop mid-search when time runs out

**Khuyến nghị:**
```
Ưu tiên: CAO (cho competitive play) | Độ khó: TRUNG BÌNH | ELO ước tính: +50-100
```

1. **Basic time management:**
   ```cpp
   // Cho time control TC (milliseconds), increment INC:
   int optimumTime = timeLeft / 30 + increment;
   int maximumTime = timeLeft / 5;
   ```

2. **Check time trong search:**
   ```cpp
   // Mỗi 4096 nodes, kiểm tra thời gian
   if (nodes % 4096 == 0 && elapsed() > maximumTime)
       stopSearch = true;
   ```

3. **Easy move detection:** Nếu best move không thay đổi qua 3+ iterations → stop sớm

---

## 8. CÁC TECHNIQUE NHỎ KHÁC

### 8A. Mate Distance Pruning
```
Ưu tiên: THẤP | Độ khó: RẤT THẤP | ELO: +2-5
```
```cpp
// Stockfish search.cpp:703-706
alpha = std::max(mated_in(ply), alpha);
beta = std::min(mate_in(ply + 1), beta);
if (alpha >= beta) return alpha;
```

### 8B. Internal Iterative Reduction (IIR)
```
Ưu tiên: THẤP | Độ khó: RẤT THẤP | ELO: +5-10
```
```cpp
// Khi không có TT move ở depth cao → reduce depth
if (depth >= 6 && !ttMove)
    depth--;
```

### 8C. Improving Flag
```
Ưu tiên: TRUNG BÌNH | Độ khó: THẤP | ELO: +10-20
```
- Track static eval ở mỗi ply
- `improving = staticEval > staticEval_2_ply_ago`
- Dùng improving để giảm futility margins và LMR

### 8D. History Aging
```
Ưu tiên: THẤP | Độ khó: RẤT THẤP | ELO: +3-5
```
```cpp
// Stockfish: decay history mỗi iteration
for (int i = 0; i < HISTORY_SIZE; i++)
    mainHistory[c][i] = mainHistory[c][i] * 820 / 1024;
```

---

## TỔNG HỢP - Roadmap Ưu Tiên

### Phase 1: Quick Wins (+150-250 ELO, ~1-2 tuần)
| # | Feature | ELO Est. | Difficulty |
|---|---------|----------|------------|
| 1 | LMR logarithmic + history-based | +100-200 | Medium |
| 2 | Futility pruning (parent + child) | +30-50 | Low |
| 3 | Razoring | +10-20 | Low |
| 4 | Null move dynamic R (3 + depth/3) | +30-50 | Low |
| 5 | SEE pruning trong qsearch | +10-20 | Low |

### Phase 2: Core Improvements (+100-200 ELO, ~2-3 tuần)
| # | Feature | ELO Est. | Difficulty |
|---|---------|----------|------------|
| 6 | TT cluster-based + generation | +50-100 | Medium |
| 7 | TT probe trong qsearch | +20-40 | Medium |
| 8 | Move ordering: capture history + SEE sort | +30-50 | Medium |
| 9 | History malus (penalty failed moves) | +10-20 | Low |
| 10 | Improving flag + integrated into pruning | +10-20 | Low |

### Phase 3: Advanced (+50-150 ELO, ~3-4 tuần)
| # | Feature | ELO Est. | Difficulty |
|---|---------|----------|------------|
| 11 | Singular extensions (simplified) | +30-50 | High |
| 12 | Continuation history (1-level) | +20-30 | Medium |
| 13 | Time management (basic) | +50-100 | Medium |
| 14 | Eval: pseudo-mobility, rook on open file | +20-40 | Medium |
| 15 | Mate distance pruning + IIR | +7-15 | Low |

### Tổng cộng ước tính: +300-600 ELO (từ ~1600 lên ~2000-2200)

---

## Lưu Ý Quan Trọng

1. **Test từng feature riêng lẻ** — thêm 1 feature, chạy test, đo ELO trước khi thêm feature tiếp
2. **Stockfish dùng NNUE** cho evaluation — các PST values của MyChess (PeSTO) vẫn rất tốt cho classical eval
3. **Material values của Stockfish** đã thay đổi nhiều so với classical: `Pawn=208, Knight=781, Bishop=825, Rook=1276, Queen=2538` — nhưng đây là cho NNUE, không nên copy
4. **Stockfish có ~20 yếu tố ảnh hưởng LMR** — bắt đầu với 3-4 yếu tố quan trọng nhất (depth, moveCount, history, improving)
5. **Không nên clear TT mỗi lần calc()** — TT nên persist giữa các nước đi
