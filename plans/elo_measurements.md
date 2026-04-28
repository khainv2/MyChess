# Kết quả đo ELO của MyChess

Phương pháp: đấu vs Stockfish (UCI_LimitStrength=true, UCI_Elo=N) qua `match.py`.
Time control: 1000 ms/move (cả hai bên), Hash 16 MB, 1 thread mỗi engine.

Công thức:
```
score = (W + 0.5·D) / N
ΔELO  = −400 · log10(1/score − 1)
ELO   = opponent_elo + ΔELO
```

## Round 1 — vs SF UCI_Elo=1800  (2026-04-27)

| W  | D | L | Score | ΔELO   | Ước ELO | 95% CI       |
|----|---|---|-------|--------|---------|--------------|
| 26 | 1 | 3 | 88.3% | +351.7 | **2152** | ±194 → [1958, 2345] |

Quan sát: score quá lệch → CI rộng. Cần đối thủ mạnh hơn để precision tốt hơn.
File log: `match_elo1800.log`.

## Round 2 — vs SF UCI_Elo=2100  (2026-04-27)

| W  | D | L | Score | ΔELO   | Ước ELO  | 95% CI       |
|----|---|---|-------|--------|----------|--------------|
| 19 | 2 | 9 | 66.7% | +120.4 | **2220** | ±132 → [2089, 2352] |

CI hẹp hơn round 1 vì score gần 50%. File log: `match_elo2100.log`.

## Tổng hợp

Hai round overlap đáng kể:
- Round 1: [1958, 2345]
- Round 2: [2089, 2352]

→ **Ước lượng MyChess ≈ 2150–2250 ELO** (tại time control 1000 ms/move).

Lưu ý: ELO scale này dựa trên `UCI_Elo` của Stockfish, không phải CCRL/FIDE chính thức. Stockfish UCI_Elo có xu hướng *underestimate* — thực tế CCRL có thể thấp hơn ~100-200 ELO.

## Cách chạy lại
```
python3 -u match.py --games 30 --movetime 1000 --sf-elo <N> --workers 8 --quiet \
  > match_elo<N>.log 2>&1
python3 elo_estimate.py --wins W --draws D --losses L --opponent-elo <N>
```

