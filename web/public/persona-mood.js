// Phase 2 — Mood layer.
// Theo dõi eval (cp score) qua thời gian, phát sinh "tâm trạng" cho persona
// dựa trên (a) xu hướng vật chất + position, (b) tâm trạng nền của ngày.
//
// Eval đến từ engine — nhưng QUAN TRỌNG: eval mà UI nhận luôn POV "bên-đang-đi".
// Cần lật dấu nếu persona không phải bên đó.
//
// Public API:
//   updateMoodFromEval({cp, mate, sideToMove}) — gọi từ app.js khi nhận info.
//   resetMood()                                 — gọi khi newGame.
//   pickDailyMood(persona)                      — bốc 1 tâm trạng nền cho session.
//   getMoodHint()                               — text inject vào system prompt.
//   currentMoodLevel()                          — string ('panic'|'tense'|...).

import { state } from './state.js';

// ── Mood state machine ─────────────────────────────────────────────────

const MOOD_DECAY_PER_PLY = 0.12;     // intensity giảm dần nếu không có cú sốc mới
const SHOCK_THRESHOLD_HI = 150;      // delta cp lớn → mood swing mạnh
const SHOCK_THRESHOLD_LO = 60;       // delta cp vừa → mood swing nhẹ
const STABLE_TURNS_FOR_BORED = 6;    // bored sau 6 turn eval gần như đứng

function moodFromEval(evalForPersona, delta) {
    if (delta >= SHOCK_THRESHOLD_HI) return { level: 'gloating', intensity: 0.9 };
    if (delta >= SHOCK_THRESHOLD_LO) return { level: 'happy',    intensity: 0.6 };
    if (delta <= -SHOCK_THRESHOLD_HI) return { level: 'panic',   intensity: 0.95 };
    if (delta <= -SHOCK_THRESHOLD_LO) return { level: 'tense',   intensity: 0.7 };
    if (Math.abs(evalForPersona) < 30) return null;  // không thay đổi
    return null;
}

function personaIsWhite() {
    // Người chơi cầm Trắng → persona Đen, và ngược lại.
    const human = document.getElementById('human-side');
    return human && human.value === 'black';  // user black → persona white
}

export function updateMoodFromEval(info) {
    if (!state.featureFlags?.mood) return;
    if (!state.currentPersona) return;
    if (!info || !info.score) return;

    // info.score: { kind: 'cp'|'mate', value: number }
    let cp;
    if (info.score.kind === 'mate') {
        // mate N from POV bên-đang-đi: dương = bên-đang-đi mate, âm = bị mate
        cp = info.score.value > 0 ? 10000 : -10000;
    } else {
        cp = info.score.value;
    }

    // POV: engine info luôn theo bên-đang-đi (bất kể trắng/đen). Lấy turn hiện
    // tại của state.game (chess.js trả 'w'|'b'). Nếu trùng persona → giữ; khác → lật.
    const turn = state.game.turn();  // 'w' | 'b'
    const personaSide = personaIsWhite() ? 'w' : 'b';
    const evalForPersona = (turn === personaSide) ? cp : -cp;

    const m = state.personaMood || {};
    const prev = m.prevEvalCp;
    const delta = (prev == null) ? 0 : evalForPersona - prev;

    const newMood = moodFromEval(evalForPersona, delta);

    if (newMood) {
        // Cú sốc mới → override mood, ghi nhớ cause
        let cause;
        if (delta >= SHOCK_THRESHOLD_HI)       cause = 'thế cờ vừa nghiêng hẳn về phía bạn';
        else if (delta >= SHOCK_THRESHOLD_LO)  cause = 'tình thế đang nhỉnh dần về phía bạn';
        else if (delta <= -SHOCK_THRESHOLD_HI) cause = 'vừa hứng đòn nặng, thế cờ xấu hẳn đi';
        else if (delta <= -SHOCK_THRESHOLD_LO) cause = 'thế cờ đang xấu dần về phía bạn';
        state.personaMood = {
            level: newMood.level,
            intensity: newMood.intensity,
            cause,
            sinceMove: state.plyCount,
            prevEvalCp: evalForPersona,
            stableSince: state.plyCount,
            lastUpdated: Date.now(),
        };
    } else {
        // Không sốc — decay intensity, kiểm tra bored.
        const stableSince = m.stableSince ?? state.plyCount;
        const stableTurns = state.plyCount - stableSince;
        let level = m.level || 'neutral';
        let intensity = Math.max(0, (m.intensity || 0) - MOOD_DECAY_PER_PLY);
        let cause = m.cause || null;

        if (intensity < 0.2 && Math.abs(evalForPersona) < 50 && stableTurns >= STABLE_TURNS_FOR_BORED) {
            level = 'bored';
            intensity = 0.4;
            cause = 'thế cờ cứ giằng co, nhạt nhạt';
        } else if (intensity < 0.15) {
            level = 'neutral';
            cause = null;
        }
        state.personaMood = {
            ...m,
            level,
            intensity,
            cause,
            prevEvalCp: evalForPersona,
            stableSince,
            lastUpdated: Date.now(),
        };
    }
}

export function resetMood() {
    state.personaMood = {
        level: 'neutral',
        intensity: 0,
        cause: null,
        sinceMove: 0,
        prevEvalCp: null,
        stableSince: 0,
        lastUpdated: 0,
    };
}

// ── Mood-of-the-day (nền) ──────────────────────────────────────────────

export function pickDailyMood(persona) {
    if (!persona || !Array.isArray(persona.moodPool) || persona.moodPool.length === 0) {
        state.personaDailyMood = null;
        return;
    }
    const idx = Math.floor(Math.random() * persona.moodPool.length);
    state.personaDailyMood = persona.moodPool[idx];
}

// ── Mood hint inject vào prompt ────────────────────────────────────────

const LEVEL_LABELS = {
    neutral:  'bình thường',
    happy:    'đang vui phấn khởi',
    gloating: 'đang đắc thắng, khoái chí, hơi vênh váo',
    tense:    'đang căng thẳng, lo lắng',
    panic:    'đang hoảng loạn, thấy thế cờ xấu hẳn',
    bored:    'đang nhạt, không hứng thú, mỏi mệt',
};

// Mỗi mood gắn 1 cách "thể hiện đầu câu" cụ thể, để bot không nói câu trung
// tính khi đang panic/gloating.
const LEVEL_OPENERS = {
    happy:    'mở câu bằng âm/từ vui (ví dụ: "ơ", "ha", "yes", "đỉnh", "ờ ngon")',
    gloating: 'mở câu bằng âm/từ đắc thắng (ví dụ: "ờ", "hà", "thấy chưa", "đó thấy chưa", "kêu mãi")',
    tense:    'mở câu bằng âm/từ lo lắng hoặc thở dài (ví dụ: "thôi rồi", "khó đây", "ơ kìa", "haizz")',
    panic:    'mở câu bằng âm/từ hoảng (ví dụ: "trời ơi", "hỏng rồi", "chết thật", "thôi xong", "hức")',
    bored:    'mở câu bằng âm/từ thờ ơ (ví dụ: "ờm", "thôi", "chán nhỉ", "haizz", "lại nữa")',
};

export function getMoodHint() {
    if (!state.featureFlags?.mood) return null;
    const m = state.personaMood;
    const daily = state.personaDailyMood;
    const lines = [];
    if (m && m.level && m.level !== 'neutral' && m.intensity > 0.15) {
        const label = LEVEL_LABELS[m.level] || m.level;
        const reason = m.cause ? ` (vì ${m.cause})` : '';
        const opener = LEVEL_OPENERS[m.level];
        lines.push(`Tâm trạng hiện tại: ${label}${reason}.`);
        lines.push(`YÊU CẦU: câu trả lời PHẢI thể hiện rõ cảm xúc này NGAY TỪ ĐẦU câu — ${opener || 'mở bằng từ thể hiện đúng tâm trạng'}. KHÔNG được mở bằng câu trung tính kiểu "ờ tớ thấy" hay "nước này khá đấy" rồi mới nói cảm xúc.`);
    }
    if (daily) {
        lines.push(`Tâm trạng nền hôm nay: bạn ${daily}. Có thể để lộ ra nếu hợp ngữ cảnh.`);
    }
    if (lines.length === 0) return null;
    return ['— Tâm trạng —', ...lines].join('\n');
}

export function currentMoodLevel() {
    return state.personaMood?.level || 'neutral';
}
