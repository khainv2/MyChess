// Auto-play test harness cho persona chat.
//
// Mô phỏng đầy đủ pipeline:
//   - Engine vs Engine 1 ván (qua WS bridge của server)
//   - Mỗi nước trigger 1 chat_request mô phỏng đúng logic persona-chat:
//       * theatrical pick (reactionWeights + anti-repeat)
//       * mood (eval-driven, decay)
//       * precompute compare (top-1 từ engine)
//   - Lưu log JSON: turn, event, reaction, mood, prompt, reply
//
// Usage:
//   node test_persona_pipeline.js [--persona=be-mochi] [--turns=20] [--out=logs/run.json]
//
// Yêu cầu: web server local đã chạy (port 8080) + vLLM tunnel.

const path = require('path');
const fs = require('fs');
const WebSocket = require(path.join(__dirname, 'node_modules', 'ws'));
const { Chess } = require(path.join(__dirname, 'node_modules', 'chess.js'));

const argv = Object.fromEntries(process.argv.slice(2)
    .map(a => a.replace(/^--/, '').split('='))
    .map(([k, v]) => [k, v ?? true]));

const PERSONA_ID = argv.persona || 'be-mochi';
const NUM_TURNS  = parseInt(argv.turns || '20', 10);
const OUT_PATH   = argv.out || `logs/persona-${PERSONA_ID}-${Date.now()}.json`;
const WS_URL     = argv.ws || 'ws://localhost:8080';
const ENGINE_MT  = parseInt(argv.movetime || '300', 10);

const EVENT_TAG = '[SỰ-KIỆN-BÀN-CỜ]';
const PERSONAS = JSON.parse(fs.readFileSync(path.join(__dirname, 'public', 'personas.json'), 'utf8'));
const persona = PERSONAS.find(p => p.id === PERSONA_ID);
if (!persona) {
    console.error(`Persona "${PERSONA_ID}" không có trong personas.json`);
    process.exit(1);
}
console.log(`▶ Persona: ${persona.emoji} ${persona.name} (elo ${persona.elo}, chattiness ${persona.chattiness})`);
console.log(`▶ Turns: ${NUM_TURNS}, output: ${OUT_PATH}\n`);

// ── Theatrical (port từ persona-theatrical.js) ──────────────────────────

const FORCE_COMMENT = new Set(['start', 'win', 'lose', 'draw', 'precognition']);
const OFF_FAMILY = ['off_topic_personal', 'meta', 'random_observation'];
const MOOD_MULTIPLIERS = {
    panic:    { silent: 2.0, bantering: 0.3, off_topic_personal: 0.6 },
    tense:    { silent: 1.5, bantering: 0.5 },
    gloating: { bantering: 2.5, comment_move: 1.3, silent: 0.4 },
    happy:    { bantering: 1.5, off_topic_personal: 1.2, silent: 0.7 },
    bored:    { off_topic_personal: 1.8, random_observation: 1.6, comment_move: 0.6 },
    neutral:  {},
};
const MOOD_HARD_BAN = {
    panic:    new Set(['question_back', 'random_observation', 'off_topic_personal', 'meta']),
    tense:    new Set(['question_back']),
    gloating: new Set(['question_back', 'random_observation']),
    bored:    new Set([]),
    happy:    new Set([]),
    neutral:  new Set([]),
};
let recentReactions = [];
let recentReplies = [];

function pickWeighted(weights) {
    let total = 0; for (const k in weights) total += Math.max(0, weights[k] || 0);
    if (total <= 0) return 'comment_move';
    let r = Math.random() * total;
    for (const k in weights) { const w = Math.max(0, weights[k] || 0); if (r < w) return k; r -= w; }
    return 'comment_move';
}
function pickReactionType(eventType, moodLevel = 'neutral') {
    if (FORCE_COMMENT.has(eventType)) return 'comment_move';
    const weights = { ...persona.reactionWeights };
    const mult = MOOD_MULTIPLIERS[moodLevel] || {};
    for (const k of Object.keys(weights)) if (typeof mult[k] === 'number') weights[k] = Math.max(0, Math.floor(weights[k] * mult[k]));
    const ban = MOOD_HARD_BAN[moodLevel] || new Set();
    for (const k of ban) weights[k] = 0;
    const last2 = recentReactions.slice(-2);
    if (last2.length === 2 && last2.every(x => x === 'comment_move')) weights.comment_move = Math.floor((weights.comment_move || 0) * 0.5);
    const offCount = recentReactions.slice(-3).filter(x => OFF_FAMILY.includes(x)).length;
    if (offCount >= 2) for (const k of OFF_FAMILY) weights[k] = 0;
    else if (recentReactions.length && OFF_FAMILY.includes(recentReactions[recentReactions.length - 1])) for (const k of OFF_FAMILY) weights[k] = Math.floor((weights[k] || 0) * 0.4);
    if (recentReactions.length && recentReactions[recentReactions.length - 1] === 'silent') weights.silent = 0;
    if (recentReactions.filter(x => x === 'silent').length >= 1) weights.silent = 0;
    return pickWeighted(weights);
}
const TOPIC_BY_CHOICE = {
    off_topic_personal: ['personal_life', 'complaints', 'boasts'],
    random_observation: ['small_talk'],
    meta:               ['hobbies', 'personal_life'],
};
let recentTopicsArr = [];
function pickTopicHint(reactionType) {
    const cats = TOPIC_BY_CHOICE[reactionType];
    if (!cats || !persona.topics) return null;
    const cat = cats[Math.floor(Math.random() * cats.length)];
    const list = persona.topics[cat];
    if (!Array.isArray(list) || list.length === 0) return null;
    const recent = new Set(recentTopicsArr);
    const candidates = list.filter(t => !recent.has(t));
    const pool = candidates.length > 0 ? candidates : list;
    const picked = pool[Math.floor(Math.random() * pool.length)];
    recentTopicsArr.push(picked);
    if (recentTopicsArr.length > 4) recentTopicsArr = recentTopicsArr.slice(-4);
    return picked;
}
const OFF_TOPIC_BAN = 'TUYỆT ĐỐI KHÔNG nhắc đến cờ vua, nước cờ, quân cờ, bàn cờ, ô bàn cờ, kết quả thắng/thua, hay bất kỳ từ nào liên quan trận đấu trong câu này.';
const POSITIVE_MOODS = new Set(['neutral', 'happy', 'gloating']);
const HEAVY_TOPIC_BAN = 'KHÔNG nói chuyện buồn nặng/mất mát/đại tang/người thân qua đời/ốm nặng — chỉ chuyện đời thường, vụn vặt, vui hoặc trung tính (đồ ăn, học hành, công việc, bạn bè, lười biếng, rảnh rỗi…).';
function distillTopic(hint) {
    if (!hint) return '';
    let s = hint.replace(/^(tớ|tao|em|cô|cụ|anh|chị|mẹ|bố|bà|ông)\s+(tớ|tao)?\s*/i, '');
    s = s.replace(/^(hôm nay|hôm qua|tối qua|sáng nay|hôm trước|tối nào|buổi chiều|cuối tuần)\s+/i, '');
    return s.split(/\s+/).slice(0, 7).join(' ');
}
function topicLine(topicHint) {
    if (!topicHint) return '';
    const seed = distillTopic(topicHint);
    return ` Ý tưởng nội dung (chỉ là gợi ý concept, KHÔNG phải template, KHÔNG được lặp nguyên văn): ${seed}. PHẢI viết câu HOÀN TOÀN MỚI theo giọng persona, dùng từ ngữ khác hẳn gợi ý.`;
}
function moodToneLine(moodLevel) { return POSITIVE_MOODS.has(moodLevel) ? ` ${HEAVY_TOPIC_BAN}` : ''; }
function describeReaction(reactionType, topicHint, moodLevel = 'neutral') {
    const moodTone = moodToneLine(moodLevel);
    switch (reactionType) {
        case 'comment_move':       return 'Hãy bình luận ngắn về nước cờ vừa xảy ra theo đúng tính cách. KHÔNG nói formal kiểu MC khai mạc ("mong chúng ta", "ván đấu sòng phẳng", "chúc may mắn"). Nói như đời thường — bạn bè đang tán dóc.';
        case 'bantering':          return 'Lần này HÃY: nói 1 câu trash-talk/đùa vô thưởng vô phạt theo đúng giọng, KHÔNG phân tích chi tiết nước cờ.';
        case 'off_topic_personal': return `Lần này HÃY: nói 1 câu bâng quơ về đời sống cá nhân, đúng giọng persona. ${OFF_TOPIC_BAN}${moodTone}${topicLine(topicHint)}`;
        case 'meta':               return `Lần này HÃY: tự nói 1 câu về bản thân/cảm xúc/sở thích. ${OFF_TOPIC_BAN}${moodTone}${topicLine(topicHint)}`;
        case 'question_back':      return `Lần này HÃY: hỏi ngược NGƯỜI CHƠI (đối thủ) 1 câu về cuộc sống của họ, đúng giọng persona. NGHIÊM: bạn LÀ persona — KHÔNG đảo vai, KHÔNG xưng "em" rồi gọi đối phương "cô". Giữ đúng đại từ persona dùng. Câu hỏi hướng VỀ đối phương, không hướng về bản thân. ${OFF_TOPIC_BAN}${moodTone}`;
        case 'random_observation': return `Lần này HÃY: nhận xét bâng quơ về thế giới xung quanh. ${OFF_TOPIC_BAN}${moodTone}${topicLine(topicHint)}`;
        case 'silent':             return null;
        default:                   return null;
    }
}

// ── Mood (port từ persona-mood.js, simplified, persona = black) ─────────

let mood = { level: 'neutral', intensity: 0, prevEvalCp: null, stableSince: 0 };
let dailyMood = persona.moodPool ? persona.moodPool[Math.floor(Math.random() * persona.moodPool.length)] : null;
let plyCount = 0;
const PERSONA_SIDE = 'b';   // hard-code persona = black trong test này

function updateMoodFromEval(scoreCp, scoreKind, turn) {
    let cp = scoreKind === 'mate' ? (scoreCp > 0 ? 10000 : -10000) : scoreCp;
    const evalForPersona = (turn === PERSONA_SIDE) ? cp : -cp;
    const prev = mood.prevEvalCp;
    const delta = (prev == null) ? 0 : evalForPersona - prev;
    let newM = null;
    if (delta >= 150)        newM = { level: 'gloating', intensity: 0.9 };
    else if (delta >= 60)    newM = { level: 'happy',    intensity: 0.6 };
    else if (delta <= -150)  newM = { level: 'panic',    intensity: 0.95 };
    else if (delta <= -60)   newM = { level: 'tense',    intensity: 0.7 };
    if (newM) {
        let cause;
        if (delta >= 150)       cause = 'thế cờ vừa nghiêng hẳn về phía bạn';
        else if (delta >= 60)   cause = 'tình thế đang nhỉnh dần về phía bạn';
        else if (delta <= -150) cause = 'vừa hứng đòn nặng, thế cờ xấu hẳn đi';
        else                    cause = 'thế cờ đang xấu dần về phía bạn';
        mood = { ...newM, cause, prevEvalCp: evalForPersona, stableSince: plyCount };
    } else {
        const stableSince = mood.stableSince ?? plyCount;
        const stableTurns = plyCount - stableSince;
        let level = mood.level || 'neutral';
        let intensity = Math.max(0, (mood.intensity || 0) - 0.12);
        let cause = mood.cause || null;
        if (intensity < 0.2 && Math.abs(evalForPersona) < 50 && stableTurns >= 6) {
            level = 'bored'; intensity = 0.4; cause = 'thế cờ cứ giằng co, nhạt nhạt';
        } else if (intensity < 0.15) {
            level = 'neutral'; cause = null;
        }
        mood = { ...mood, level, intensity, cause, prevEvalCp: evalForPersona, stableSince };
    }
}
const LEVEL_OPENERS = {
    happy:    'mở câu bằng âm/từ vui (ví dụ: "ơ", "ha", "yes", "đỉnh", "ờ ngon")',
    gloating: 'mở câu bằng âm/từ đắc thắng (ví dụ: "ờ", "hà", "thấy chưa", "đó thấy chưa")',
    tense:    'mở câu bằng âm/từ lo lắng (ví dụ: "thôi rồi", "khó đây", "haizz")',
    panic:    'mở câu bằng âm/từ hoảng (ví dụ: "trời ơi", "hỏng rồi", "chết thật", "thôi xong", "hức")',
    bored:    'mở câu bằng âm/từ thờ ơ (ví dụ: "ờm", "thôi", "chán nhỉ", "haizz")',
};
function getMoodHint() {
    const lines = [];
    const LABELS = { neutral:'bình thường', happy:'đang vui phấn khởi', gloating:'đang đắc thắng, vênh váo', tense:'đang căng thẳng', panic:'đang hoảng loạn', bored:'đang nhạt, mỏi mệt' };
    if (mood.level !== 'neutral' && mood.intensity > 0.15) {
        lines.push(`Tâm trạng hiện tại: ${LABELS[mood.level]}${mood.cause ? ' (vì ' + mood.cause + ')' : ''}.`);
        const opener = LEVEL_OPENERS[mood.level];
        lines.push(`YÊU CẦU: câu trả lời PHẢI thể hiện rõ cảm xúc này NGAY TỪ ĐẦU câu — ${opener || 'mở bằng từ thể hiện đúng tâm trạng'}. KHÔNG được mở câu trung tính rồi mới nói cảm xúc.`);
    }
    if (dailyMood) lines.push(`Tâm trạng nền hôm nay: bạn ${dailyMood}.`);
    if (lines.length === 0) return null;
    return ['— Tâm trạng —', ...lines].join('\n');
}

// ── Prompt builders (port persona-chat.js) ──────────────────────────────

function pieceVN(t) { return ({ p:'Tốt', n:'Mã', b:'Tượng', r:'Xe', q:'Hậu', k:'Vua' })[t] || t; }
function describeMove(result, eventType, inCheck) {
    const parts = [`đi ${pieceVN(result.piece)} đến ô ${result.to}`];
    if (result.captured) parts.push(`ăn ${pieceVN(result.captured)} của đối thủ`);
    if (result.promotion) parts.push(`phong cấp thành ${pieceVN(result.promotion)}`);
    const f = result.flags || '';
    if (f.includes('k')) parts.push('nhập thành cánh Vua');
    if (f.includes('q')) parts.push('nhập thành cánh Hậu');
    if (f.includes('e')) parts.push('bắt Tốt qua đường');
    if (inCheck) parts.push(eventType === 'engine_move' ? 'nước này chiếu vua đối thủ' : 'nước này chiếu vua của bạn');
    return parts.join(', ');
}
function materialBalance(game) {
    const VAL = { p:1, n:3, b:3, r:5, q:9, k:0 };
    let p = 0, o = 0;
    const board = game.board();
    for (let r = 0; r < 8; r++) for (let c = 0; c < 8; c++) {
        const sq = board[r][c]; if (!sq) continue;
        const v = VAL[sq.type] || 0;
        if (sq.color === PERSONA_SIDE) p += v; else o += v;
    }
    return p - o;
}
function matDesc(game) {
    const d = materialBalance(game);
    if (d >=  5) return 'bạn đang ưu thế quân lớn';
    if (d >=  2) return 'bạn đang nhỉnh hơn về quân';
    if (d >=  1) return 'bạn đang nhỉnh hơn 1 chút';
    if (d <= -5) return 'bạn đang lép vế quân lớn';
    if (d <= -2) return 'bạn đang kém hơn về quân';
    if (d <= -1) return 'bạn đang kém hơn 1 chút';
    return 'thế cờ cân bằng về quân';
}
function buildAntiRepeatBlock() {
    if (recentReplies.length === 0) return null;
    const list = recentReplies.slice(-3).map((r, i) => `${i + 1}. "${r}"`).join('\n');
    const lastReply = recentReplies[recentReplies.length - 1] || '';
    const lastOpener = lastReply.trim().split(/\s+/).slice(0, 3).join(' ');
    return [
        '— Câu trả lời gần đây CỦA CHÍNH BẠN (TUYỆT ĐỐI KHÔNG LẶP LẠI) —',
        list,
        'Quy tắc nghiêm:',
        '1. Câu sắp tới PHẢI có cấu trúc + cụm từ chính KHÁC HẲN các câu trên. KHÔNG mở câu cùng kiểu.',
        '2. KHÔNG lặp signature/catchphrase đã dùng (vd "non lắm", "trời ơi", "ơ kìa", "ehe", "mẹ tớ bảo", "thế á", "hì hì", "thử lại đi"...).',
        lastOpener ? `3. ĐẶC BIỆT: TUYỆT ĐỐI KHÔNG mở câu sắp tới bằng cụm "${lastOpener}" (đã dùng câu trước).` : '',
        '4. Đa dạng chủ đề — không nói về cùng đối tượng 2 câu liên tiếp.',
    ].filter(Boolean).join('\n');
}
function buildSystemPrompt() {
    const moodHint = getMoodHint();
    const antiRepeat = buildAntiRepeatBlock();
    const lines = [
        persona.system_prompt,
        '',
        `Bối cảnh trận đấu: bạn cầm bên Đen. Đối thủ của bạn (người chơi) cầm bên Trắng.`,
    ];
    if (moodHint) lines.push('', moodHint);
    if (antiRepeat) lines.push('', antiRepeat);
    lines.push(
        '',
        `QUY ƯỚC: tin nhắn user bắt đầu bằng ${EVENT_TAG} là tường thuật sự kiện cờ — hãy bình luận 1 câu ngắn về sự kiện đó.`,
        '',
        'Yêu cầu nghiêm ngặt khi bình luận sự kiện cờ:',
        '- Trả lời thật ngắn 1 câu (tối đa 2 câu). Tiếng Việt thuần. Không emoji, không markdown, không xuống dòng.',
        '- TUYỆT ĐỐI KHÔNG dùng ký hiệu cờ vua tiếng Anh: KHÔNG nói "Nf3", "Bxc6+", "Qd5", "O-O", "e2e4". Nếu cần nói tên quân thì dùng tiếng Việt: "Mã", "Tượng", "Hậu", "Xe", "Vua", "Tốt". Tên ô (e4, f6, c5...) là OK vì chỉ là toạ độ.',
        '- TUYỆT ĐỐI KHÔNG bịa quân cờ chưa nêu trong tin user.',
        '- KHÔNG copy nguyên văn từ ngữ trong tin user — phải diễn giải theo giọng của bạn.',
    );
    return lines.join('\n');
}
function buildEventPrompt(eventType, ctx, directive) {
    const movePhase = plyCount <= 10 ? 'khai cuộc' : plyCount <= 30 ? 'trung cuộc' : 'cuối ván';
    const md = matDesc(ctx.game);
    const TAG = EVENT_TAG + ' ';
    if (eventType === 'start') return TAG + `Ván đấu vừa bắt đầu. Hãy chào đối thủ một câu thật ngắn theo đúng tính cách. KHÔNG nói formal kiểu MC khai mạc ("mong chúng ta", "ván đấu sòng phẳng", "chúc may mắn", "ván cờ mới mở ra"). Chỉ là chào đời thường khi gặp bạn quen.`;
    if (eventType === 'engine_move') {
        return TAG + [
            `Bạn vừa ${describeMove(ctx.result, eventType, ctx.inCheck)}.`,
            `Tình thế tổng quan: ${md}.`,
            `Đang ở giai đoạn: ${movePhase}.`,
            directive || `Hãy nói 1 câu thật ngắn theo đúng tính cách.`,
        ].join(' ');
    }
    if (eventType === 'user_move') {
        const parts = [
            `Đối thủ vừa ${describeMove(ctx.result, eventType, ctx.inCheck)}.`,
            `Tình thế tổng quan: ${md}.`,
            `Đang ở giai đoạn: ${movePhase}.`,
        ];
        const mq = ctx.moveQuality;
        if (mq && mq.quality === 'matched' && ctx.lastQ !== 'matched') parts.push('Cảm nhận nội bộ: nước đối thủ vừa đi đúng kỳ vọng của bạn.');
        else if (mq && mq.quality === 'differed' && ctx.lastQ !== 'differed') parts.push('Cảm nhận nội bộ: nước đối thủ đi hơi bất ngờ.');
        parts.push(directive || `Hãy phản ứng 1 câu ngắn theo đúng tính cách. KHÔNG bắt đầu câu bằng "ờ" hay "ơ tớ tưởng".`);
        return TAG + parts.join(' ');
    }
    return TAG + `Hãy nói 1 câu phù hợp.`;
}

// ── shouldChat (gate) ───────────────────────────────────────────────────

let lastChatPly = -10;
function moveImportance(result, inCheck) {
    if (!result) return 0;
    let s = 0;
    if (result.captured) s += ({q:80,r:50,b:32,n:32,p:12}[result.captured]) || 0;
    const f = result.flags || '';
    if (f.includes('e')) s += 18;
    if (result.promotion) s += 70;
    if (f.includes('k') || f.includes('q')) s += 40;
    if (inCheck) s += 55;
    if (plyCount <= 3) s += 28;
    if (plyCount - lastChatPly >= 9) s += 25;
    return s;
}
function shouldChat(eventType, result, inCheck) {
    if (['start','win','lose','draw','precognition'].includes(eventType)) return true;
    const sinceLast = plyCount - lastChatPly;
    if (sinceLast >= 2 && Math.random() < (persona.chattiness || 0.5) * 0.18) return true;
    let threshold = 80 - (persona.chattiness || 0.5) * 50;
    if (sinceLast <= 1) threshold += 35;
    else if (sinceLast === 2) threshold += 15;
    return moveImportance(result, inCheck) >= threshold;
}

// ── Heuristic judge (port từ persona-judge.js) ──────────────────────────

const SAN_RE = /(?:^|\s)([NBRQK][a-h]?[1-8]?x?[a-h][1-8](?:=[NBRQ])?[+#]?|O-O(?:-O)?|[a-h][1-8][a-h][1-8])(?:\s|$|[.,!?])/;
const EN_PIECE_RE = /\b(knights?|bishops?|rooks?|queens?|kings?|pawns?)\b/i;
const FORBIDDEN_OPENERS = [/^ờ\s+t[ưu]/i, /^ơ\s+t[ưu]/i, /^ờ\s+nước/i, /^ơ\s+nước/i];
const CHESS_TOKEN_RE = /(cờ\s*vua|nước\s*cờ|quân\s*cờ|bàn\s*cờ|chiếu\s*vua|chiếu\s*tướng|chiếu\s*hết|nhập\s*thành|phong\s*cấp|ăn\s*tốt|ăn\s*mã|ăn\s*tượng|ăn\s*hậu|ăn\s*xe|ăn\s*vua|nước\s*này|nước\s*đi)/i;
const MOOD_LITERAL = ['hoảng loạn', 'đắc thắng', 'vênh váo', 'phấn khởi', 'căng thẳng', 'mỏi mệt', 'khoái chí', 'lo lắng'];
const OFF_REACTIONS = new Set(['off_topic_personal', 'meta', 'random_observation', 'question_back']);

function jNorm(s) { return (s || '').toLowerCase().replace(/[.,!?…"'":;()\[\]{}]/g, ' ').replace(/\s+/g, ' ').trim(); }
function jTokens(s) { return new Set(jNorm(s).split(' ').filter(t => t.length > 1)); }
function jOverlap(a, b) {
    const A = jTokens(a), B = jTokens(b);
    if (!A.size || !B.size) return 0;
    let inter = 0; for (const t of A) if (B.has(t)) inter++;
    const uni = A.size + B.size - inter;
    return uni > 0 ? inter / uni : 0;
}
function jHead(s, n = 3) { return jNorm(s).split(' ').slice(0, n).join(' '); }
function jBigrams(s) {
    const ts = jNorm(s).split(' ').filter(t => t.length > 0);
    const out = [];
    for (let i = 0; i < ts.length - 1; i++) out.push(ts[i] + ' ' + ts[i + 1]);
    return out;
}
const SIGNATURE_TOKENS = [
    'ehe', 'ehee', 'ehehe', 'hì', 'hì hì', 'hihi', 'hihihi',
    'hà hà', 'haha', 'hahaha', 'haizz', 'haiz', 'hức', 'hứt',
    'oách xà lách', 'oa', 'ơ kìa', 'trời ơi', 'chết thật',
    'đỡ chứ', 'non lắm', 'gà thế', 'vãi cả', 'đỉnh',
    'chà chà', 'hừm',
];

function scoreReply(text, ctx = {}) {
    const { recentReplies = [], reactionType = 'comment_move', personaSignatures = [], topicHint = null } = ctx;
    if (!text || typeof text !== 'string') return { score: -1000, reasons: ['empty'] };
    const trimmed = text.trim();
    if (!trimmed) return { score: -1000, reasons: ['empty'] };
    let score = 100; const reasons = []; const lower = trimmed.toLowerCase();
    if (trimmed.length < 8)   { score -= 35; reasons.push('too_short'); }
    if (trimmed.length > 240) { score -= 25; reasons.push('long'); }
    if (trimmed.length > 320) { score -= 30; reasons.push('way_too_long'); }
    if (SAN_RE.test(' ' + trimmed + ' ')) { score -= 60; reasons.push('san_leak'); }
    if (EN_PIECE_RE.test(trimmed))         { score -= 45; reasons.push('english_piece'); }
    if (/[*_`#]{1,}/.test(trimmed))                         { score -= 25; reasons.push('markdown'); }
    if (/[\u{1F300}-\u{1FAFF}]|[\u{2600}-\u{27BF}]/u.test(trimmed)) { score -= 20; reasons.push('emoji'); }
    if (/^["'"'"]/.test(trimmed) || /["'"'"]$/.test(trimmed))      { score -= 12; reasons.push('wrap_quotes'); }
    for (const re of FORBIDDEN_OPENERS) if (re.test(trimmed)) { score -= 30; reasons.push('bad_opener'); break; }
    if (OFF_REACTIONS.has(reactionType) && CHESS_TOKEN_RE.test(lower)) { score -= 55; reasons.push('off_topic_with_chess'); }
    for (const lit of MOOD_LITERAL) if (lower.includes(lit)) { score -= 25; reasons.push('mood_literal'); break; }
    let maxOv = 0, sameStart = false;
    const head = jHead(trimmed, 3);
    for (const r of recentReplies) {
        const ov = jOverlap(trimmed, r);
        if (ov > maxOv) maxOv = ov;
        if (head.length > 0 && jHead(r, 3) === head) sameStart = true;
    }
    if (maxOv > 0.5)        { score -= 55; reasons.push('high_overlap'); }
    else if (maxOv > 0.35)  { score -= 28; reasons.push('mid_overlap'); }
    if (sameStart)          { score -= 22; reasons.push('same_opener_as_recent'); }
    const allSignatures = [...SIGNATURE_TOKENS, ...personaSignatures.map(s => s.toLowerCase())];
    let sigHits = 0;
    for (const sig of allSignatures) {
        if (!sig || !lower.includes(sig)) continue;
        for (const r of recentReplies) if ((r || '').toLowerCase().includes(sig)) { sigHits++; break; }
    }
    if (sigHits >= 2)       { score -= 50; reasons.push('signature_overuse_heavy'); }
    else if (sigHits >= 1)  { score -= 28; reasons.push('signature_overuse'); }
    let inSentenceDup = 0;
    for (const sig of allSignatures) {
        if (!sig) continue;
        let count = 0, idx = 0;
        while ((idx = lower.indexOf(sig, idx)) !== -1) { count++; idx += sig.length; }
        if (count >= 2) inSentenceDup++;
    }
    if (inSentenceDup >= 1) { score -= 30; reasons.push('signature_dup_in_sentence'); }
    if (topicHint) {
        const hintLower = topicHint.toLowerCase();
        const hintWords = hintLower.split(/\s+/).filter(w => w.length > 1);
        if (hintWords.length >= 4) {
            let matched4 = false;
            for (let i = 0; i + 4 <= hintWords.length; i++) {
                const ng = hintWords.slice(i, i + 4).join(' ');
                if (lower.includes(ng)) { matched4 = true; break; }
            }
            if (matched4) { score -= 60; reasons.push('topic_hint_copied'); }
        }
        const ov = jOverlap(trimmed, topicHint);
        if (ov > 0.55)        { score -= 45; reasons.push('topic_hint_high_overlap'); }
        else if (ov > 0.4)    { score -= 22; reasons.push('topic_hint_overlap'); }
    }
    const myBg = new Set(jBigrams(trimmed));
    let bgHits = 0;
    for (const r of recentReplies) for (const bg of jBigrams(r)) if (myBg.has(bg)) bgHits++;
    if (bgHits >= 4)        { score -= 45; reasons.push('bigram_repeat_heavy'); }
    else if (bgHits >= 2)   { score -= 22; reasons.push('bigram_repeat'); }
    let stuffCount = 0;
    for (const sig of SIGNATURE_TOKENS) if (lower.includes(sig)) stuffCount++;
    if (stuffCount >= 3)    { score -= 35; reasons.push('catchphrase_stuffed'); }
    const words = jNorm(trimmed).split(' ');
    const wfreq = {};
    for (const w of words) if (w.length > 2) wfreq[w] = (wfreq[w] || 0) + 1;
    const maxFreq = Math.max(0, ...Object.values(wfreq));
    if (maxFreq >= 4) { score -= 25; reasons.push('repeat_word'); }
    const sentences = trimmed.split(/[.!?…]+/).filter(s => s.trim().length > 0);
    if (sentences.length > 3) { score -= 15; reasons.push('too_many_sentences'); }
    const lastChar = trimmed.slice(-1);
    const endsWithPunct = /[.!?…)\]"'"'"]/.test(lastChar);
    const tail = trimmed.split(/\s+/).slice(-2).join(' ').toLowerCase();
    const TRAILING_INCOMPLETE = /\b(là|của|với|và|hay|như|cho|về|từ|tại|theo|trên|dưới|trong|ngoài|nhất|hơn|một|hai|ba|bốn|năm)$/;
    const lastWord = trimmed.split(/\s+/).pop() || '';
    if (!endsWithPunct && trimmed.length > 30) {
        if (TRAILING_INCOMPLETE.test(tail) || lastWord.length <= 2) {
            score -= 40; reasons.push('truncated_sentence');
        } else {
            score -= 8; reasons.push('no_end_punct');
        }
    }
    if (/\b(t[ớo]|c[ậa]u|em|anh|ch[ịi]|m[ìi]nh|b[ạa]n)\b/i.test(lower)) score += 4;
    if (trimmed.length >= 25 && trimmed.length <= 160) score += 6;
    return { score, reasons };
}

function pickBest(candidates, ctx) {
    if (!candidates.length) return null;
    let bestIdx = -1, bestScore = -Infinity;
    const all = [];
    for (let i = 0; i < candidates.length; i++) {
        const c = candidates[i];
        if (!c || !c.text) continue;
        const s = scoreReply(c.text, ctx);
        all.push({ i, score: s.score, reasons: s.reasons });
        if (s.score > bestScore) { bestScore = s.score; bestIdx = i; }
    }
    return { idx: bestIdx, score: bestScore, all };
}

// ── WebSocket round-trip helpers ────────────────────────────────────────

function withTimeout(p, ms, label) {
    return Promise.race([p, new Promise((_, rej) => setTimeout(() => rej(new Error(`${label} timeout`)), ms))]);
}

class WSClient {
    constructor(url) {
        this.ws = new WebSocket(url);
        this.pendingBest = null;
        this.pendingChat = new Map();
        this.readyP = new Promise((resolve, reject) => {
            this.ws.on('open', () => {});
            this.ws.on('message', (d) => {
                const m = JSON.parse(d.toString());
                if (m.type === 'ready') resolve();
                else if (m.type === 'bestmove') { if (this.pendingBest) { const cb = this.pendingBest; this.pendingBest = null; cb(m); } }
                else if (m.type === 'chat') { const cb = this.pendingChat.get(m.requestId); if (cb) { this.pendingChat.delete(m.requestId); cb(m); } }
                // ignore info / error
            });
            this.ws.on('error', reject);
        });
    }
    waitReady() { return withTimeout(this.readyP, 5000, 'ws ready'); }
    newgame() { this.ws.send(JSON.stringify({ type: 'newgame' })); }
    position(fen, moves) { this.ws.send(JSON.stringify({ type: 'position', fen, moves })); }
    go(movetimeMs) {
        return withTimeout(new Promise(res => { this.pendingBest = res; this.ws.send(JSON.stringify({ type: 'go', movetimeMs })); }), 30000, 'bestmove');
    }
    chat(requestId, messages, maxTokens, opts = {}) {
        const payload = { type: 'chat_request', requestId, messages, maxTokens };
        if (typeof opts.temperature === 'number') payload.temperature = opts.temperature;
        if (typeof opts.seed === 'number')        payload.seed        = opts.seed;
        return withTimeout(new Promise(res => { this.pendingChat.set(requestId, res); this.ws.send(JSON.stringify(payload)); }), 25000, 'chat');
    }
    close() { try { this.ws.close(); } catch (_) {} }
}

// ── Main runner ─────────────────────────────────────────────────────────

const log = [];
let chatHistory = [];

async function runChat(eventType, ctx) {
    if (!shouldChat(eventType, ctx.result, ctx.inCheck)) {
        log.push({ ply: plyCount, event: eventType, action: 'skip_should_chat', mood: { ...mood }, dailyMood });
        return;
    }
    const reactionType = pickReactionType(eventType, mood.level);
    if (reactionType === 'silent') {
        recentReactions.push(reactionType); recentReactions = recentReactions.slice(-4);
        lastChatPly = plyCount;
        log.push({ ply: plyCount, event: eventType, reaction: 'silent', mood: { ...mood }, dailyMood });
        return;
    }
    const topicHint = pickTopicHint(reactionType);
    const directive = describeReaction(reactionType, topicHint, mood.level);
    lastChatPly = plyCount;
    const sys = buildSystemPrompt();
    const usr = buildEventPrompt(eventType, ctx, directive);
    const messages = [{ role:'system', content: sys }, ...chatHistory, { role:'user', content: usr }];
    const baseSeed = Math.floor(Math.random() * 1e9);
    const reqA = `pipeline-${plyCount}-${eventType}-A`;
    const reqB = `pipeline-${plyCount}-${eventType}-B`;
    let reply = '';
    let judgeDebug = null;
    try {
        const settled = await Promise.allSettled([
            ws.chat(reqA, messages, 160, { temperature: 0.85, seed: baseSeed }),
            ws.chat(reqB, messages, 160, { temperature: 1.05, seed: baseSeed + 7919 }),
        ]);
        const cands = [];
        for (let i = 0; i < settled.length; i++) {
            const s = settled[i];
            if (s.status === 'fulfilled' && s.value && s.value.ok && s.value.text) {
                cands.push({ text: s.value.text.trim(), variant: i });
            }
        }
        if (cands.length === 0) {
            reply = `(both candidates failed)`;
        } else if (cands.length === 1) {
            reply = cands[0].text;
            judgeDebug = { single: true, variant: cands[0].variant };
        } else {
            const judgeCtx = {
                recentReplies, reactionType,
                personaSignatures: persona.signaturePhrases || [],
                topicHint,
            };
            const pick = pickBest(cands, judgeCtx);
            reply = cands[pick.idx].text;
            judgeDebug = { picked: pick.idx, scores: pick.all };
        }
    } catch (e) { reply = `(timeout: ${e.message})`; }
    chatHistory.push({ role: 'user', content: usr });
    chatHistory.push({ role: 'assistant', content: reply });
    if (chatHistory.length > 60 * 2) chatHistory = chatHistory.slice(-60 * 2);
    recentReactions.push(reactionType); recentReactions = recentReactions.slice(-4);
    recentReplies.push(reply); recentReplies = recentReplies.slice(-3);
    log.push({
        ply: plyCount, event: eventType, reaction: reactionType,
        mood: { level: mood.level, intensity: +mood.intensity.toFixed(2), cause: mood.cause },
        dailyMood, topic: topicHint, directive: directive?.slice(0, 80),
        prompt_user: usr, reply, judge: judgeDebug,
    });
    const dbg = judgeDebug?.scores
        ? ` [pick=#${judgeDebug.picked} ${judgeDebug.scores.map(s => s.score).join('/')}]`
        : (judgeDebug?.single ? ` [single]` : '');
    process.stdout.write(`  [${plyCount.toString().padStart(2)}] ${eventType.padEnd(12)} ${reactionType.padEnd(20)} mood=${mood.level.padEnd(8)} → ${reply}${dbg}\n`);
}

let ws;
(async () => {
    ws = new WSClient(WS_URL);
    await ws.waitReady();
    ws.newgame();
    console.log('Engine ready, bắt đầu EvE...\n');

    const game = new Chess();
    const moves = [];
    await runChat('start', { game, history: [], result: null, inCheck: false });

    for (let i = 0; i < NUM_TURNS; i++) {
        if (game.isGameOver()) break;
        ws.position(undefined, moves);
        const bm = await ws.go(ENGINE_MT);
        if (!bm.uci) break;
        const turnBefore = game.turn();
        const move = game.move({ from: bm.uci.slice(0,2), to: bm.uci.slice(2,4), promotion: bm.uci.length > 4 ? bm.uci[4] : undefined });
        if (!move) break;
        moves.push(bm.uci);
        plyCount++;

        // Mock mood update bằng heuristic eval (server không gửi lại info trong test này; estimate đơn giản theo material)
        const matCp = materialBalance(game) * 100;  // crude
        updateMoodFromEval(matCp, 'cp', game.turn());

        const inCheck = game.inCheck();
        const isPersonaMove = (turnBefore === PERSONA_SIDE);
        const ev = isPersonaMove ? 'engine_move' : 'user_move';
        const lastQ = log.slice().reverse().find(x => x.event === 'user_move')?.moveQuality;
        await runChat(ev, { game, history: moves.map((_, idx) => game.history()[idx] || ''), result: move, inCheck, lastQ });
    }

    if (game.isGameOver()) {
        let endEvent = 'draw';
        if (game.isCheckmate()) endEvent = (game.turn() === PERSONA_SIDE) ? 'lose' : 'win';
        await runChat(endEvent, { game, history: game.history(), result: null, inCheck: game.inCheck() });
    }

    fs.mkdirSync(path.dirname(path.resolve(OUT_PATH)), { recursive: true });
    fs.writeFileSync(OUT_PATH, JSON.stringify({
        persona: { id: persona.id, name: persona.name, elo: persona.elo, chattiness: persona.chattiness },
        dailyMood,
        finalFen: game.fen(),
        movesSan: game.history(),
        log,
    }, null, 2));
    console.log(`\n✓ Log saved: ${OUT_PATH}`);
    console.log(`  Total turns: ${plyCount} | chat events: ${log.filter(x => x.reply).length} | silent: ${log.filter(x => x.reaction === 'silent').length} | skipped: ${log.filter(x => x.action === 'skip_should_chat').length}`);
    ws.close();
    process.exit(0);
})().catch(e => { console.error('FATAL:', e); process.exit(1); });
