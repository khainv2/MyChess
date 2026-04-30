# Persona Enhancement — Kế hoạch nâng cấp nhân vật bot

**Trạng thái**: Draft, chờ user duyệt
**Tác giả**: Claude (đề xuất)
**Phạm vi**: `web/public/persona-chat.js`, `web/public/personas.json`, `web/server.js`, các file phụ trợ mới
**Không đụng**: engine C++ (`algorithm/`), giao thức UCI

---

## 1. Tổng quan

Hiện tại các persona LLM đã có giọng riêng nhờ `system_prompt` tĩnh, nhưng phản ứng vẫn "một màu":
- **Stateless**: mỗi turn xuất phát từ baseline tính cách giống hệt nhau, không có cảm xúc tích lũy.
- **Đơn nguyên**: chỉ phản ứng với event vừa xảy ra, không có "agenda" của riêng bot.
- **Nguồn input nghèo**: chỉ có FEN/SAN/material làm context; thiếu thời gian, lịch sử người chơi, sự kiện ngẫu nhiên.
- **Bị ép luôn nói về cờ**: mọi prompt event đều yêu cầu *"hãy bình luận nước cờ"* → bot không có cơ hội bâng quơ.

Kế hoạch này tăng độ thú vị của persona thông qua **5 phase**, mỗi phase ship được độc lập, có thể tắt/bật bằng feature flag.

---

## 2. Mục tiêu & success criteria

### Mục tiêu
1. **Đa dạng**: trong 20 nước liên tiếp với cùng 1 persona, không có 2 câu nào lặp pattern (chào → bình luận → chào → bình luận).
2. **Có "đời sống"**: persona phản ứng với thời gian thực (giờ, ngày), nhớ ván trước, có tâm trạng thay đổi theo eval.
3. **Có ý kiến riêng về cờ**: bot có thể "tiên tri" hoặc nhận xét trước nước người chơi sẽ đi (dùng engine pre-compute).
4. **Không bám cờ liên tục**: phải có turn bot nói chuyện hoàn toàn off-topic (đời sống, thời tiết, cảm xúc) — đúng kiểu người thật.

### Success criteria (định lượng)
- **Phase 1**: Trong test 30 turn liên tiếp với Bé Mochi, ≥ 30% turn không nhắc nước cờ cụ thể nào.
- **Phase 2**: Câu trả lời khi eval thay đổi >150cp khác rõ rệt với khi eval ổn định (manual review 10 cặp).
- **Phase 3**: Trong 10 ván liên tiếp, có ≥ 3 lần persona nói trước khi user đi (tiên tri) và ≥ 5 lần phản ứng so sánh với pre-computed top move.
- **Phase 4**: Ván thứ 2 trở đi với cùng persona, có nhắc đến ván trước trong ≥ 30% câu chào đầu ván.

---

## 3. Vấn đề cốt lõi cần giải

| # | Vấn đề | Phase giải |
|---|--------|------------|
| P1 | Bot luôn cố nhồi nhét cờ vào mọi câu | Phase 1 (theatrical choice) |
| P2 | Không có chủ đề off-topic | Phase 1 (topic bank) |
| P3 | Cảm xúc không thay đổi trong ván | Phase 2 (mood layer) |
| P4 | Không nhận xét được nước người chơi sắp đi | Phase 3 (engine pre-compute) |
| P5 | Mỗi ván reset về 0, không có liên tục | Phase 4 (cross-game memory) |
| P6 | Không có nhịp riêng (chỉ phản ứng sự kiện) | Phase 5 (idle trigger, tics) |

---

## 4. Kiến trúc tổng quan

### Module mới (frontend)
```
web/public/
├── persona-chat.js          (đã có — refactor để gọi các module mới)
├── persona-theatrical.js    (NEW) — pick reaction type + topic hint
├── persona-mood.js          (NEW) — track eval delta, output mood word
├── persona-memory.js        (NEW) — localStorage cross-game stats
├── persona-precompute.js    (NEW) — request top moves từ engine, cache
└── personas.json            (mở rộng schema)
```

### Module backend
```
web/server.js
├── (đã có) chat_request → callLLM
├── (NEW) precompute_request → spawn parallel engine call (movetime ngắn)
└── (NEW) extend chat_request: nhận temperature, seed, max_tokens động
```

### Data flow mới (turn engine_move)

```
engine emit bestmove
   ↓
app.js applyEngineMove()
   ↓
   ├─→ persona-mood.js: cập nhật mood từ eval delta
   ├─→ persona-memory.js: ghi sự kiện (capture, promotion)
   └─→ persona-chat.js: triggerPersonaChat('engine_move', ctx)
            ↓
            ├─→ persona-theatrical.js: pickChoice() → 'comment_move' | 'off_topic' | ...
            ├─→ persona-mood.js: getMoodHint() → 'gloating', 'panic', ...
            ├─→ persona-memory.js: getRivalryHint() → "ván trước thua quân lớn"
            └─→ build prompt với tất cả các hint trên
                ↓
            requestChat → server → vLLM → reply
```

### Data flow mới (lượt user)

```
applyEngineMove() done
   ↓
schedule precompute (300ms after settle)
   ↓
persona-precompute.js: request top 3 moves cho phía user
   ↓
cache state.preComputed
   ↓
[5s timer]: nếu user vẫn chưa đi & dice < 0.15
   → triggerPersonaChat('precognition', { topMoves })
```

---

## 5. Schema mở rộng

### `personas.json` — thêm field

```jsonc
{
  "id": "be-mochi",
  "name": "Bé Mochi",
  // ... fields cũ ...

  // NEW — trọng số thể loại reaction (Phase 1)
  "reactionWeights": {
    "comment_move":      35,
    "bantering":         20,
    "off_topic_personal":15,
    "meta":              10,
    "question_back":      8,
    "random_observation": 7,
    "silent":             5
  },

  // NEW — kho chủ đề off-topic (Phase 1)
  "topics": {
    "personal_life": [
      "vừa rớt môn giải tích cao cấp",
      "stream Pewpew tối qua hay quá",
      "deadline đồ án tuần này chết mất"
    ],
    "complaints":    ["lab Toán ức chế quá", "phòng trọ mưa dột"],
    "boasts":        ["mới mua được váy đẹp", "vừa được 9 môn Anh"],
    "small_talk":    ["trời đang chuyển hè nhỉ", "đói quá"]
  },

  // NEW — câu cửa miệng (Phase 5)
  "tics": ["trời ơi", "ehe", "ơ kìa", "oách xà lách"],

  // NEW — temperature riêng cho LLM (Phase 5)
  "llm": { "temperature": 0.95, "topP": 0.92 },

  // NEW — mood-of-the-day pool (Phase 2)
  "moodPool": [
    "đang vui vì mới ăn trà sữa",
    "đang cáu vì mất ngủ",
    "đang yêu (crush mới quen)",
    "bình thường"
  ]
}
```

### `state.js` — thêm field

```js
// Phase 2
state.personaMood = {
  level: 'neutral',     // 'happy'|'tense'|'panic'|'gloating'|'sulking'|'bored'
  intensity: 0,         // 0..1
  cause: null,          // text
  sinceMove: 0,         // ply khi mood update
  prevEvalCp: null,     // để tính delta
  lastUpdated: 0
};

// Phase 2 — mood-of-the-day cố định trong session
state.personaDailyMood = null;

// Phase 3
state.preComputed = {
  forFen: null,         // FEN tại thời điểm tính
  topMoves: [],         // [{ uci, san, scoreCp }, ...]
  computedAt: 0
};

// Phase 4 — load từ localStorage một lần
state.personaMemory = null;  // { [personaId]: { games: [...], stats: {...} } }
```

### `localStorage` — Phase 4

Key: `mychess.persona_memory.v1`
Format:
```jsonc
{
  "be-mochi": {
    "stats": { "wins": 3, "losses": 1, "draws": 0 },
    "games": [
      {
        "result": "loss",
        "endedAt": 1735632000000,
        "moves": 47,
        "opening": "e4 e5 Nf3",
        "moments": ["lost_queen_early", "long_endgame"]
      }
      // tối đa 5 ván gần nhất, rotate FIFO
    ],
    "lastSeen": 1735632000000
  }
}
```

Tổng dung lượng dự kiến: < 10KB cho cả 7 persona.

---

## 6. Phase 1 — Theatrical Choice + Topic Bank (MVP)

**Đòn bẩy lớn nhất / chi phí nhỏ nhất.** Ship trước.

### Scope
- Thêm logic random "thể loại reaction" trước khi build prompt.
- Bốc ngẫu nhiên 1 chủ đề từ `topics` của persona.
- Chỉnh `buildEventPrompt` và `buildSystemPromptForFreeChat` để inject các hint trên.
- Hỗ trợ thể loại `silent` → skip không gọi LLM.

### Files
- `personas.json`: thêm `reactionWeights`, `topics` cho cả 7 persona.
- `persona-theatrical.js` (NEW, ~80 dòng):
  ```js
  export function pickReactionType(persona, eventType, ctx) {...}  // weighted random
  export function pickTopicHint(persona, reactionType) {...}        // sample từ kho
  export function describeReaction(reactionType) {...}              // text inject
  ```
- `persona-chat.js`:
  - `triggerPersonaChat`: gọi `pickReactionType` đầu hàm; nếu `silent` → return ngay.
  - `buildEventPrompt(eventType, ctx, choice, topicHint)`: thêm khối hint cuối prompt.

### Prompt design (sửa `buildEventPrompt`)

Trước:
```
[SỰ-KIỆN-BÀN-CỜ] Đối thủ vừa đi quân Mã Nf6. Tình thế: cân bằng. Lịch sử: e4 e5 Nf3 Nc6 Bb5 Nf6.
Hãy phản ứng 1 câu ngắn theo đúng tính cách (chê, khen, trêu, hoặc dửng dưng — tùy bạn).
```

Sau (ví dụ choice=`off_topic_personal`):
```
[SỰ-KIỆN-BÀN-CỜ] Đối thủ vừa đi quân Mã Nf6. Tình thế: cân bằng. Lịch sử: e4 e5 Nf3 Nc6 Bb5 Nf6.

Lần này HÃY: bâng quơ kể chuyện đời tư, KHÔNG bình luận về nước cờ.
Gợi ý chủ đề: "vừa rớt môn giải tích cao cấp".
Hãy nói 1 câu thật ngắn theo đúng tính cách, lồng chủ đề vào tự nhiên.
```

### Quy tắc bốc choice
- `start`/`win`/`lose`/`draw` → luôn `comment_move` (event quan trọng, cần phản ứng đúng).
- `engine_move` / `user_move` → bốc theo `reactionWeights` của persona.
- Nếu 2 lần liên tiếp đã chọn `comment_move` → giảm trọng số `comment_move` 50% lần thứ 3 (forced rotation).
- `silent` → không gọi LLM, không hiện gì trong chat (cảm giác bot "tự hiểu" không phải lúc nào cũng phải nói).

### Test
- `web/test_theatrical.js` (NEW): mock random, gọi `pickReactionType` 1000 lần, kiểm tra phân phối khớp weights ±5%.
- Manual: chơi 1 ván 30 nước với Bé Mochi, log tất cả `(choice, prompt, reply)`. Check ≥ 30% turn không có tên quân/SAN.

### Risks & mitigation
- **Risk**: bot không thể vừa nói off-topic vừa giữ giọng → kiểm tra prompt trước khi ship; thêm `"vẫn giữ đúng giọng/dialect persona"` vào prompt.
- **Risk**: `silent` quá nhiều → cảm giác bot lười → cap weight `silent` ≤ 8% và không cho 2 turn `silent` liên tiếp.

### Dự kiến: 1 ngày code + nửa ngày tune

---

## 7. Phase 2 — Mood Layer (eval-driven cảm xúc)

### Scope
- Theo dõi eval (cp score) qua các nước, tính delta theo POV persona.
- Sinh "mood word" + "intensity" + "cause".
- Inject vào system prompt mỗi turn.
- Bốc 1 "mood-of-the-day" cố định khi load persona, để baseline khác nhau giữa các session.

### Files
- `persona-mood.js` (NEW, ~120 dòng):
  ```js
  export function updateMoodFromEval(currentEvalCp);
  export function getMoodHint();          // { line: string, intensity: number }
  export function pickDailyMood(persona); // bốc 1 từ moodPool
  ```
- `app.js`: trong handler `info` (eval stream), gọi `updateMoodFromEval`.
- `persona-chat.js`: `buildSystemPrompt` & `buildSystemPromptForFreeChat` thêm dòng mood hint.

### Logic mood update

```js
// Mỗi khi nhận eval mới (cp từ POV bên-đang-đi)
const evalForPersona = personaIsBlack ? -cp : cp;
const delta = evalForPersona - state.personaMood.prevEvalCp;

if (delta > 150)        mood = 'gloating';   // vừa lên dây cót
else if (delta > 60)    mood = 'happy';
else if (delta < -150)  mood = 'panic';
else if (delta < -60)   mood = 'tense';
else if (|evalForPersona| < 30 && stableTurns > 5) mood = 'bored';
else                    mood = 'neutral';

// intensity decay theo thời gian (mỗi nước -0.1)
```

### Prompt injection

Thêm vào system prompt (sau `buildGameBackgroundBlock`):
```
— Tâm trạng hiện tại —
Bạn đang [panic] (vì vừa mất Hậu cách 2 nước, cường độ cao). Hãy phản ứng đúng cảm xúc đó trong câu trả lời.
Tâm trạng nền hôm nay: [đang cáu vì mất ngủ].
```

### Tương tác với Phase 1
Mood ảnh hưởng `reactionWeights`:
- `panic`/`tense` → giảm `bantering`, `boasts`; tăng `silent`, `complaints`.
- `gloating` → tăng `bantering`, `boasts`; giảm `silent`.
- `bored` → tăng `off_topic_personal`, `random_observation`.

### Test
- Manual scenario 1: chơi 1 ván rồi cố tình thí Hậu — kiểm tra bot chuyển sang giọng "panic"/"sulking".
- Manual scenario 2: mid-game ở vị thế cân bằng nhiều nước → kiểm tra bot có lúc dùng giọng "bored" + off-topic.

### Risks & mitigation
- **Risk**: eval bouncing nhiều giữa nước (hash collision, depth khác nhau) → smooth bằng EMA hoặc chỉ cập nhật mood khi delta giữ ≥ 2 nước.
- **Risk**: persona Cu Tí 600 elo không nên hiểu eval → giải pháp: cause text dùng từ ngữ ngây thơ ("thấy sờ sợ", "thấy đỡ rồi") thay vì "eval -200cp".

### Dự kiến: 2 ngày code + 1 ngày tune prompt

---

## 8. Phase 3 — Engine Pre-compute (tiên tri & nhận xét nước sắp đi)

**Phức tạp nhất**, đòi hỏi đụng cả backend.

### Scope
- Khi đến lượt user, server spawn 1 engine call song song để tính top 3 nước cho **phía user** với `movetime 300-500`.
- Cache vào `state.preComputed`.
- 2 use cases:
  1. **Tiên tri**: 5s sau lượt user mà chưa đi → có xác suất bot nói trước "tớ đoán cậu sẽ đi X" hoặc "đi gì cũng được tớ vẫn hơn".
  2. **So sánh hậu kiểm**: khi user đi xong, so sánh với top moves → bot phản ứng phù hợp.

### Files
- `server.js`: thêm message type `precompute_request`. Cần process engine thứ 2 hoặc dùng "go" trên engine hiện tại trong khe trống (engine hiện đang idle khi đến lượt user — dùng được).
- `persona-precompute.js` (NEW, ~100 dòng):
  ```js
  export function requestPrecompute(fen);   // gửi WS request
  export function handlePrecomputeResponse(msg);
  export function getPrecomputed();         // đọc cache
  export function compareUserMove(uciPlayed); // 'top1'|'top3'|'blunder'|'unique_winning'
  ```
- `app.js`: trigger precompute sau khi engine_move done; trigger `triggerPersonaChat('precognition')` qua timer 5s.

### Backend design

Thêm vào `server.js` message handler:
```js
case 'precompute_request': {
  // {requestId, fen}
  // Re-use engine instance: send `position fen ...; go movetime 400 multipv 3`
  // Collect 3 PVs, send back as {type:'precompute', requestId, topMoves}
}
```

Vấn đề: engine MyChess hỗ trợ `multipv` không?
- **Cần check**: nếu không, fallback dùng 3 lần `go movetime 130` với position khác nhau (mỗi lần exclude best vừa tìm) — chậm hơn nhưng vẫn dưới 500ms.
- Hoặc: chấp nhận chỉ top-1 (dễ implement), top-2 và top-3 dùng heuristic chess.js (random từ legal moves).

**Action item**: trước khi implement Phase 3, run `printf 'uci\nposition startpos\ngo movetime 500 multipv 3\n' | _build_uci_nodeps/MyChessUCI` để kiểm tra.

### Prompt design — tiên tri

Event mới `precognition`:
```
[SỰ-KIỆN-BÀN-CỜ] Đến lượt đối thủ đi (5 giây rồi vẫn chưa đi).
Phân tích máy của bạn: nước hay nhất họ nên đi là Nf3, các nước hay khác là Bc4 và d4.
Lần này HÃY: nói 1 câu kiểu "tiên tri" hoặc trêu chọc. KHÔNG được lộ chính xác nước cờ "Nf3" — chỉ ám chỉ chung chung ("chắc là đẩy Mã ra"), hoặc nói về kết cục chung ("đi gì tớ cũng vẫn hơn").
```

### Prompt design — so sánh hậu kiểm

Sửa `buildEventPrompt('user_move', ctx)`:
```
[SỰ-KIỆN-BÀN-CỜ] Đối thủ vừa đi Nf6.
[Engine pre-compute đã có sẵn cho lượt vừa rồi: top1=Nf3 (cp=+15), top2=Bc4 (cp=+10), top3=d4 (cp=+8). Nước thực đi: Nf6 — không nằm trong top 3, cp=-30 → CHẤT LƯỢNG: kém]
Lần này HÃY: phản ứng 1 câu ngắn dựa trên việc đối thủ vừa đi nước kém chất lượng. KHÔNG đem tên nước "Nf3"/"Bc4" ra nói. Có thể dùng cảm tính ("ơ nước này không ổn", "thấy ghê chưa").
```

### Quy tắc tiên tri (an toàn)
- **Không** kích hoạt nếu top1 cách top2 ≥ 100cp (= chỉ có 1 nước thắng duy nhất, lộ ra sẽ ruin trải nghiệm).
- Xác suất kích hoạt: 15% × `chattiness` của persona, max 1 lần / 5 nước.
- Có thể kích hoạt qua user setting `enable_precognition` (off mặc định cho người mới).

### Test
- `web/test_precompute.js` (NEW): test backend trả về top moves đúng format.
- Manual: chơi với Hải Sát Thủ, kích hoạt nhiều lần → kiểm tra không lộ chính xác best move.

### Risks & mitigation
- **Risk**: engine pre-compute chậm khi user đi nhanh → hủy request cũ trước khi gửi mới (debounce).
- **Risk**: lộ best move giảm tính giáo dục → quy tắc safety + opt-in setting.
- **Risk**: tốn CPU cho mỗi WS connection → chỉ pre-compute khi mode=PvE và persona đã chọn.

### Dự kiến: 3-4 ngày (1 ngày check engine multipv + 2 ngày code + 1 ngày tune)

---

## 9. Phase 4 — Cross-game Memory + Realtime

### Scope
- localStorage lưu lịch sử ván trước per-persona.
- Inject "rivalry hint" vào system prompt đầu ván & các turn quan trọng.
- Inject thời gian thực (giờ, ngày, thứ) vào system prompt.

### Files
- `persona-memory.js` (NEW, ~100 dòng):
  ```js
  export function loadMemory();
  export function saveMemory();
  export function recordGameEnd(personaId, result, summary);
  export function getRivalryHint(personaId);  // text để inject
  ```
- `persona-chat.js`: `buildSystemPrompt*` thêm rivalryHint + realtime hint.
- `app.js`: cuối ván, gọi `recordGameEnd`.

### Realtime hint

```js
function getRealtimeHint() {
  const now = new Date();
  const hour = now.getHours();
  const dayOfWeek = ['CN','T2','T3','T4','T5','T6','T7'][now.getDay()];
  const partOfDay = hour < 6 ? 'sáng sớm' : hour < 12 ? 'sáng' : hour < 18 ? 'chiều' : hour < 22 ? 'tối' : 'đêm khuya';
  return `Bối cảnh thời gian: ${partOfDay} ${dayOfWeek}, ${now.getHours()}:${String(now.getMinutes()).padStart(2,'0')}.`;
}
```

Inject vào cả 2 luồng prompt. LLM rất giỏi tự lái nội dung phù hợp ("đêm khuya rồi mà vẫn cày à").

### Rivalry hint (đầu ván)

```
— Lịch sử với người chơi này —
Đây là ván thứ 5. Tỷ số 3-1 nghiêng về người chơi.
Ván trước (3 ngày trước): bạn thua sau 47 nước, bị đối thủ thí Hậu sớm rồi tấn công Vua.
Hãy nhớ điều này nếu thấy phù hợp khi chào hỏi đầu ván.
```

### Moments ghi nhận
- `lost_queen_early` (mất Hậu trong 15 nước đầu)
- `lost_queen_late`
- `promoted_to_queen`
- `long_endgame` (>60 nước)
- `quick_resign` (resign trong 20 nước)
- `won_with_pawn_promotion`

### Test
- Manual: chơi 3 ván liên tiếp với Bé Mochi → ván 4 đầu ván check bot có nhắc đến ván trước.

### Risks & mitigation
- **Risk**: localStorage mất khi đổi browser → user nhận đời sống persona reset → chấp nhận, không sync server.
- **Risk**: prompt dài hơn nữa → cap moments ≤ 3 mục, chỉ inject ở event `start` & `precognition` (không phải mỗi turn).

### Dự kiến: 2 ngày

---

## 10. Phase 5 — Polish (idle trigger, tics, temperature, sub-personality)

### Scope
- **Idle trigger**: timer client-side, sau 30s lượt user không đi → có xác suất trigger event mới `user_idle`.
- **Catchphrase tics**: thỉnh thoảng inject hint *"trong câu trả lời lần này, hãy chèn cụm 'X' tự nhiên"* (mỗi 4-5 turn 1 lần).
- **Per-persona LLM params**: pass `temperature`, `top_p` qua `chat_request`. Cu Tí: 1.0; Anh Cảnh: 0.5.
- **Sub-personality kích hoạt**: khi bị chiếu liên tiếp / đầu ván / cuối ván dài → inject "sub-mood" tạm thời.

### Files
- `persona-chat.js`: thêm `tickIdleTimer`, gọi `triggerPersonaChat('user_idle')`.
- `server.js`: extend `chat_request` accept `{temperature, topP, seed}`.
- `personas.json`: thêm `llm: {temperature, topP}` per persona.

### Risks
- Idle quá nhiều → bot lải nhải → cap 1 lần / 60s. User toggle off được trong setting (nếu cần).

### Dự kiến: 1-2 ngày

---

## 11. Test strategy chung

### Unit tests
- `web/test_theatrical.js`: phân phối weights.
- `web/test_mood.js`: state machine với eval input giả lập.
- `web/test_memory.js`: load/save/rotate localStorage.

### Integration tests
- `web/test_persona_pipeline.js` (NEW): mock LLM (return canned), simulate 1 ván 40 nước, dump full prompt cho mỗi turn vào file. Diff với snapshot baseline.
- Tận dụng `test_ws_full.js` hiện có cho engine pipeline.

### Manual review
- Mỗi phase: chơi 1 ván 30+ nước với 3 persona khác phong cách (Cu Tí, Cụ Đồ, Hải Sát Thủ).
- Log full prompt + reply, đọc lại để đánh giá.
- Lưu vào `web/logs/persona-review-phase{N}-YYYYMMDD.txt`.

### Feature flags
Thêm vào `state.js`:
```js
state.featureFlags = {
  theatrical: true,    // Phase 1
  mood: true,          // Phase 2
  precompute: false,   // Phase 3 — opt-in
  memory: true,        // Phase 4
  idleTrigger: true,   // Phase 5
};
```

Đặt qua `localStorage` để dev/test bật-tắt từng phase độc lập.

---

## 12. Lộ trình & deliverables

| Phase | Tên | Tuần | Risk | Deliverables |
|-------|-----|------|------|--------------|
| 1 | Theatrical + Topics | 1 | Thấp | `personas.json` mở rộng, `persona-theatrical.js`, sửa `persona-chat.js` |
| 2 | Mood layer | 2 | Trung | `persona-mood.js`, hook eval stream |
| 3 | Engine pre-compute | 3-4 | Cao (đụng backend) | `server.js` extend, `persona-precompute.js` |
| 4 | Cross-game memory + realtime | 5 | Thấp | `persona-memory.js`, localStorage schema |
| 5 | Polish | 6 | Thấp | idle timer, tics, temperature |

Tổng: ~6 tuần nếu làm part-time. Mỗi phase ship được ngay lên prod (port 3049) thông qua feature flag.

---

## 13. Rollback plan

Mọi phase đều có flag tắt được. Nếu Phase N gây regression:
1. Đặt flag = false trong `state.js`.
2. Reload browser — quay về behavior cũ ngay.
3. Logic mới được isolate trong file riêng → không cần revert commit để tắt nhanh.

Persona memory localStorage có thể clear bằng:
```
localStorage.removeItem('mychess.persona_memory.v1')
```

---

## 14. Câu hỏi cần user duyệt trước khi code

1. **Phase 3 mặc định off hay on?** Tôi đề xuất **off** vì có thể lộ thông tin cờ. User bật ở Setting nếu muốn.
2. **Có thêm UI Setting page** để toggle các feature flag không, hay để dev-only qua localStorage?
3. **Persona memory: clear khi đổi user/browser** — chấp nhận không sync server? Hay thêm auth nhẹ để sync?
4. **Topic bank — viết thuần tay** hay cho phép user gen thêm topic qua LLM offline?
5. **Order phase**: làm theo 1→5, hay nhảy thẳng Phase 3 (vì user thích pre-compute nhất)?

## 14b. Quyết định user (2026-04-28)

1. **Phase 3 mặc định theo elo persona**:
   - Persona elo ≤ 1500 (Cu Tí 600, Tùng Trẩu 1100, Bé Mochi 1300, Cô Hằng 1500): **ON**
   - Persona elo > 1500 (Cụ Đồ 1700, Anh Cảnh 1900, Hải Sát Thủ 2000): **OFF**
   - Rationale: cao thủ không cần engine assist nhận xét nước đối thủ (đã đủ "có thẩm quyền" qua giọng); persona thấp dùng pre-compute để có cảm giác *"biết một chút về cờ kiểu gà"* — duyên hơn.
   - Implement: thêm field `enablePrecompute: bool` mặc định trong `personas.json`, hoặc derive tự động từ `elo <= 1500`.

2. **UI Setting**: thêm vào mục `<details class="details-card">` (đã có sẵn — "Chi tiết engine (nâng cao)") trong `index.html`. Có toggle riêng cho từng feature flag (theatrical / mood / precompute / memory / idle).

3. **Persona memory**: chấp nhận reset khi đổi browser. User sẽ lên phương án sync server + username sau — **out of scope** plan này.

4. **Topic bank**: spawn agent gen nội dung phong phú, output ra `web/public/persona-content-draft.json`, sau đó merge thủ công vào `personas.json`. Agent gen luôn `tics` (Phase 5) và `moodPool` (Phase 2) để khỏi phải gen lại lần nữa.

5. **Order**: làm theo thứ tự **1 → 5**.

---

## 15. Sau khi xong toàn bộ — ý tưởng tương lai

(Không trong scope plan này, ghi lại để khỏi quên)

- **Persona xen ngang**: persona khác nhảy vào chat 1 câu (Hải Sát Thủ chen ngang ván Bé Mochi).
- **Nhân vật biết nhau**: Bé Mochi nhắc đến Cụ Đồ nếu user vừa chơi với Cụ Đồ.
- **Voice**: synth giọng cho từng persona qua TTS.
- **Persona học từ user**: phát hiện opening user hay dùng → bot điều chỉnh comment.
- **Mood-of-day cố định 24h**: persistent qua session, mỗi sáng bot có "tâm trạng mới".
