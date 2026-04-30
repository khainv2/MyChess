// LLM persona logic: load personas, build system/event prompts, talk to vLLM
// over the WS bridge. Pure logic — UI surface lives in chat-panel.js.
import { state, send, MAX_CHAT_TURNS, EVENT_TAG } from './state.js';
import {
    appendChatMsg,
    clearChatLog,
    showTypingIndicator,
    hideTypingIndicator,
    setChatStatus,
    refreshChatInputState,
} from './chat-panel.js';
import { renderPlayers } from './board-render.js';
import {
    pickReactionType,
    pickTopicHint,
    describeReaction,
    rememberReaction,
    resetReactionHistory,
} from './persona-theatrical.js';
import {
    pickDailyMood,
    resetMood,
    getMoodHint,
    currentMoodLevel,
} from './persona-mood.js';
import { getPrecognitionHint } from './persona-precompute.js';
import { pickBest } from './persona-judge.js';

const personaEl        = document.getElementById('persona');
const personaSummaryEl = document.getElementById('persona-summary');
const humanSideEl      = document.getElementById('human-side');
const eloSliderEl      = document.getElementById('elo-slider');

let chatReqSeq = 0;
const pendingChatRequests = new Map();

// Pluggable side-effect: app.js sets this so we can refresh the Elo label
// after a persona pins the Elo slider.
let onEloPinned = () => {};
export function setEloLabelHandler(fn) { onEloPinned = fn; }

// ── Persona JSON ───────────────────────────────────────────────────

export async function loadPersonas() {
    try {
        const res = await fetch('personas.json', { cache: 'no-cache' });
        state.personas = await res.json();
    } catch (e) {
        state.personas = [];
        console.warn('Không tải được personas.json', e);
    }
    while (personaEl.options.length > 1) personaEl.remove(1);
    for (const p of state.personas) {
        const opt = document.createElement('option');
        opt.value = p.id;
        opt.textContent = `${p.emoji} ${p.name}`;
        personaEl.appendChild(opt);
    }
    const saved = localStorage.getItem('mychess.persona');
    if (saved && state.personas.some(p => p.id === saved)) {
        personaEl.value = saved;
    }
    applyPersonaSelection();
}

export function applyPersonaSelection() {
    const id = personaEl.value;
    const prev = state.currentPersona;
    state.currentPersona = state.personas.find(p => p.id === id) || null;
    localStorage.setItem('mychess.persona', id || '');
    if (state.currentPersona) {
        eloSliderEl.value = String(state.currentPersona.elo);
        onEloPinned();
        personaSummaryEl.style.display = '';
        personaSummaryEl.textContent = `${state.currentPersona.emoji} ${state.currentPersona.summary}`;
    } else {
        personaSummaryEl.style.display = 'none';
        personaSummaryEl.textContent = '';
    }
    if (!prev || !state.currentPersona || prev.id !== state.currentPersona.id) {
        state.chatHistory = [];
        state.recentReplies = [];
        resetReactionHistory();
        resetMood();
        pickDailyMood(state.currentPersona);
        clearChatLog();
        if (state.currentPersona) {
            appendChatMsg('system',
                `— Đã chọn ${state.currentPersona.emoji} ${state.currentPersona.name} —`);
        }
    }
    refreshChatInputState();
    renderPlayers();
}

personaEl.addEventListener('change', applyPersonaSelection);

// ── Vietnamese piece names + game-state descriptors ────────────────

function pieceNameVN(type) {
    return ({ p: 'Tốt', n: 'Mã', b: 'Tượng', r: 'Xe', q: 'Hậu', k: 'Vua' })[type] || type;
}

function materialBalanceForPersona() {
    if (!state.currentPersona) return 0;
    const personaColor = humanSideEl.value === 'white' ? 'b' : 'w';
    const VALUES = { p: 1, n: 3, b: 3, r: 5, q: 9, k: 0 };
    let pSum = 0, oSum = 0;
    const board = state.game.board();
    for (let r = 0; r < 8; r++) for (let c = 0; c < 8; c++) {
        const sq = board[r][c];
        if (!sq) continue;
        const v = VALUES[sq.type] || 0;
        if (sq.color === personaColor) pSum += v;
        else oSum += v;
    }
    return pSum - oSum;
}

function materialDescription() {
    const d = materialBalanceForPersona();
    if (d >=  5) return 'bạn đang ưu thế quân lớn';
    if (d >=  2) return 'bạn đang nhỉnh hơn về quân';
    if (d >=  1) return 'bạn đang nhỉnh hơn 1 chút';
    if (d <= -5) return 'bạn đang lép vế quân lớn';
    if (d <= -2) return 'bạn đang kém hơn về quân';
    if (d <= -1) return 'bạn đang kém hơn 1 chút';
    return 'thế cờ cân bằng về quân';
}

function describeMove(result, eventType) {
    if (!result) return '';
    // Mô tả nước cờ thuần Việt — TUYỆT ĐỐI không cho LLM thấy SAN/UCI để
    // tránh nó copy "Nf3"/"Bxc6+" vào câu trả lời. Tên ô (e4, f6) là OK
    // vì đó chỉ là toạ độ, không phải ký hiệu quân.
    const parts = [`đi ${pieceNameVN(result.piece)} đến ô ${result.to}`];
    if (result.captured)  parts.push(`ăn ${pieceNameVN(result.captured)} của đối thủ`);
    if (result.promotion) parts.push(`phong cấp thành ${pieceNameVN(result.promotion)}`);
    const flags = result.flags || '';
    if (flags.includes('k')) parts.push('nhập thành cánh Vua');
    if (flags.includes('q')) parts.push('nhập thành cánh Hậu');
    if (flags.includes('e')) parts.push('bắt Tốt qua đường');
    if (state.game.inCheck()) {
        parts.push(eventType === 'engine_move'
            ? 'nước này chiếu vua đối thủ'
            : 'nước này chiếu vua của bạn');
    }
    return parts.join(', ');
}

// ── Prompt builders ────────────────────────────────────────────────

function buildAntiRepeatBlock() {
    const recent = state.recentReplies || [];
    if (recent.length === 0) return null;
    const list = recent.slice(-3).map((r, i) => `${i + 1}. "${r}"`).join('\n');
    // Extract 3 từ đầu của câu cuối — bot tuyệt đối không mở câu giống vậy.
    const lastReply = recent[recent.length - 1] || '';
    const lastOpener = lastReply.trim().split(/\s+/).slice(0, 3).join(' ');
    return [
        '— Câu trả lời gần đây CỦA CHÍNH BẠN (TUYỆT ĐỐI KHÔNG LẶP LẠI) —',
        list,
        'Quy tắc nghiêm:',
        '1. Câu sắp tới PHẢI có cấu trúc + cụm từ chính KHÁC HẲN các câu trên. KHÔNG mở câu cùng kiểu.',
        '2. KHÔNG lặp signature/catchphrase đã dùng trong các câu trên (vd "non lắm", "đỡ nổi không", "trời ơi", "ơ kìa", "ehe", "mẹ tớ bảo", "thế á", "hì hì", "thử lại đi"...).',
        lastOpener ? `3. ĐẶC BIỆT: TUYỆT ĐỐI KHÔNG mở câu sắp tới bằng cụm "${lastOpener}" (đã dùng câu trước).` : '',
        '4. Đa dạng chủ đề — không nói về cùng đối tượng (mẹ, lớp, anh trai, crush, drama…) 2 câu liên tiếp.',
    ].filter(Boolean).join('\n');
}

function buildSystemPrompt(persona) {
    const humanColor   = humanSideEl.value === 'white' ? 'Trắng' : 'Đen';
    const personaColor = humanSideEl.value === 'white' ? 'Đen'   : 'Trắng';
    const moodHint = getMoodHint();
    const antiRepeat = buildAntiRepeatBlock();
    const lines = [
        persona.system_prompt,
        '',
        `Bối cảnh trận đấu: bạn cầm bên ${personaColor}. Đối thủ của bạn (người chơi) cầm bên ${humanColor}.`,
    ];
    if (moodHint) {
        lines.push('', moodHint);
    }
    if (antiRepeat) {
        lines.push('', antiRepeat);
    }
    lines.push(
        '',
        `QUY ƯỚC: tin nhắn user bắt đầu bằng ${EVENT_TAG} là tường thuật sự kiện cờ — hãy bình luận 1 câu ngắn về sự kiện đó. Tin user KHÔNG có tag là CHAT TRỰC TIẾP của người chơi (xem khối quy tắc free-chat ở dưới nếu có).`,
        '',
        'Yêu cầu nghiêm ngặt khi bình luận sự kiện cờ:',
        '- Trả lời thật ngắn 1 câu (tối đa 2 câu). Tiếng Việt thuần. Không emoji, không markdown, không xuống dòng.',
        '- TUYỆT ĐỐI KHÔNG dùng ký hiệu cờ vua tiếng Anh: KHÔNG nói "Nf3", "Bxc6+", "Qd5", "O-O", "e2e4". Nếu cần nói tên quân thì dùng tiếng Việt: "Mã", "Tượng", "Hậu", "Xe", "Vua", "Tốt". Tên ô (e4, f6, c5...) là OK vì chỉ là toạ độ.',
        '- TUYỆT ĐỐI KHÔNG bịa quân cờ chưa nêu trong tin user. Nếu không chắc, nói chung chung ("lại đi rồi", "thấy ghê chưa", "không tệ").',
        '- KHÔNG copy nguyên văn từ ngữ trong tin user — phải diễn giải theo giọng của bạn.',
    );
    return lines.join('\n');
}

function buildEventPrompt(eventType, ctx, reactionDirective) {
    // Không gửi SAN history nguyên văn — bot có xu hướng copy. Chỉ gửi số
    // nước đã đi để bot biết phase ván (khai cuộc/trung cuộc/cuối).
    const movePhase = state.plyCount <= 10 ? 'khai cuộc' : state.plyCount <= 30 ? 'trung cuộc' : 'cuối ván';
    const matDesc  = materialDescription();
    const TAG = EVENT_TAG + ' ';
    // reactionDirective: text inject (Phase 1) — null khi feature flag tắt.
    const directiveSuffix = reactionDirective ? ` ${reactionDirective}` : '';

    switch (eventType) {
        case 'start':
            return TAG + `Ván đấu vừa bắt đầu. Hãy chào đối thủ một câu thật ngắn theo đúng tính cách của bạn. KHÔNG nói formal kiểu MC khai mạc/chúc tụng tournament ("mong chúng ta", "ván đấu sòng phẳng", "chúc may mắn", "ván cờ mới mở ra"). Chỉ là chào đời thường khi gặp bạn quen — không phải lễ khai mạc.`;
        case 'engine_move':
            return TAG + [
                `Bạn vừa ${describeMove(ctx.result, eventType)}.`,
                `Tình thế tổng quan: ${matDesc}.`,
                `Đang ở giai đoạn: ${movePhase}.`,
                reactionDirective
                    ? reactionDirective
                    : `Hãy nói 1 câu thật ngắn theo đúng tính cách (trash-talk nhẹ, bình luận về nước mình, hoặc tự nói chuyện). Không phân tích chuyên môn dài dòng.`,
            ].join(' ');
        case 'user_move': {
            const parts = [
                `Đối thủ vừa ${describeMove(ctx.result, eventType)}.`,
                `Tình thế tổng quan: ${matDesc}.`,
                `Đang ở giai đoạn: ${movePhase}.`,
            ];
            // Phase 3: precompute compare → chỉ áp dụng khi không lặp + có ngữ
            // cảnh đáng nói. Suppress nếu turn trước đã dùng cùng quality, hoặc
            // gặp matched mỗi nước (boring).
            const mq = ctx.moveQuality;
            const lastQ = state.lastMoveQuality;
            const useHint = mq && mq.quality !== 'unknown' && mq.quality !== lastQ;
            if (useHint && mq.quality === 'matched') {
                parts.push('Cảm nhận nội bộ: nước đối thủ vừa đi đúng kỳ vọng của bạn.');
            } else if (useHint && mq.quality === 'differed') {
                parts.push('Cảm nhận nội bộ: nước đối thủ đi hơi bất ngờ, không phải nước bạn đoán họ sẽ chơi.');
            }
            state.lastMoveQuality = mq?.quality || null;
            parts.push(reactionDirective
                ? reactionDirective
                : `Hãy phản ứng 1 câu ngắn theo đúng tính cách (chê, khen, trêu, hoặc dửng dưng — tùy bạn). KHÔNG bắt đầu câu bằng "ờ" hay "ơ tớ tưởng".`);
            return TAG + parts.join(' ');
        }
        case 'precognition': {
            const hint = getPrecognitionHint();
            const parts = [
                `Đối thủ đang nghĩ lâu mà chưa đi.`,
                `Đang ở giai đoạn: ${movePhase}.`,
            ];
            if (hint) {
                parts.push(`Cảm nhận nội bộ về thế cờ: ${hint.evalDesc}.`);
            }
            parts.push(`Hãy nói 1 câu thúc giục/trêu chọc nhẹ theo đúng tính cách. Tuyệt đối KHÔNG nói tên quân hay nước cờ cụ thể.`);
            return TAG + parts.join(' ');
        }
        case 'win':
            return TAG + `Bạn vừa thắng ván này (${matDesc}). Nói 1 câu cảm nghĩ thật ngắn theo tính cách.${directiveSuffix}`;
        case 'lose':
            return TAG + `Bạn vừa thua ván này. Nói 1 câu phản ứng thật ngắn theo tính cách (cay cú, chấp nhận, đùa cợt — tùy bạn).${directiveSuffix}`;
        case 'draw':
            return TAG + `Ván cờ vừa hòa (${matDesc}). Nói 1 câu thật ngắn theo tính cách.${directiveSuffix}`;
        default:
            return TAG + `Hãy nói 1 câu phù hợp tình huống.${directiveSuffix}`;
    }
}

function buildGameBackgroundBlock() {
    const humColor  = humanSideEl.value === 'white' ? 'Trắng' : 'Đen';
    const persColor = humanSideEl.value === 'white' ? 'Đen'   : 'Trắng';
    const turn = state.game.turn() === 'w' ? 'Trắng' : 'Đen';
    const movePhase = state.plyCount <= 10 ? 'khai cuộc' : state.plyCount <= 30 ? 'trung cuộc' : 'cuối ván';
    const matDesc = materialDescription();
    return [
        '— Bối cảnh trận đấu (CHỈ tham khảo nội bộ; KHÔNG đem ra nói nếu người chơi không hỏi về cờ) —',
        `Bạn cầm bên ${persColor}, người chơi cầm bên ${humColor}.`,
        `Lượt hiện tại: ${turn} đi.`,
        `Giai đoạn: ${movePhase} (${state.plyCount} nước).`,
        `Vật chất: ${matDesc}.`,
        // Không gửi SAN history hay FEN — bot có xu hướng copy ký hiệu Anh.
    ].join('\n');
}

function buildSystemPromptForFreeChat(persona) {
    const moodHint = getMoodHint();
    const antiRepeat = buildAntiRepeatBlock();
    const lines = [
        persona.system_prompt,
        '',
        buildGameBackgroundBlock(),
    ];
    if (moodHint) lines.push('', moodHint);
    if (antiRepeat) lines.push('', antiRepeat);
    lines.push(
        '',
        `QUY ƯỚC: trong lịch sử trò chuyện, các tin user bắt đầu bằng ${EVENT_TAG} là tường thuật sự kiện cờ (auto). Tin user CUỐI hiện tại KHÔNG có tag → đó là CHAT TRỰC TIẾP của người chơi gửi cho bạn.`,
        '',
        '— QUY TẮC TRẢ LỜI CHAT TRỰC TIẾP —',
        '1. Nếu người chơi nói chuyện thường (chào hỏi, hỏi thăm, hỏi tên/tuổi, tâm sự, đùa giỡn): trả lời TỰ NHIÊN NHƯ NGƯỜI BÌNH THƯỜNG, đúng tính cách. TUYỆT ĐỐI KHÔNG đột ngột lôi nước cờ ra phân tích, KHÔNG kể "vừa rồi e4 rồi Nf3...". Ai chào thì chào lại, ai hỏi thì trả lời câu hỏi đó.',
        '2. Chỉ nói về cờ khi người chơi HỎI rõ về cờ ("thế cờ sao rồi?", "tớ vừa đi sai à?", "ăn được con gì không?"...).',
        '3. Tối đa 1-3 câu ngắn, tự nhiên, đúng giọng/dialect persona.',
        '4. KHÔNG markdown, KHÔNG emoji, KHÔNG xuống dòng nhiều, KHÔNG dấu ngoặc kép quanh cả câu.',
        '5. KHÔNG bịa quân hay sự kiện không có trong lịch sử SAN ở trên.',
        '6. KHÔNG nói thay người chơi, KHÔNG đặt câu hỏi tu từ rồi tự trả lời.',
    );
    return lines.join('\n');
}

// ── WS chat request/response ───────────────────────────────────────

function requestChat(messages, maxTokens = 110, opts = {}) {
    return new Promise((resolve, reject) => {
        if (!state.ws || state.ws.readyState !== WebSocket.OPEN) {
            reject(new Error('ws not open'));
            return;
        }
        const requestId = `chat-${++chatReqSeq}`;
        const timer = setTimeout(() => {
            pendingChatRequests.delete(requestId);
            reject(new Error('timeout'));
        }, 12000);
        pendingChatRequests.set(requestId, (msg) => {
            clearTimeout(timer);
            if (msg.ok) resolve(msg.text);
            else reject(new Error(msg.error || 'chat failed'));
        });
        const payload = { type: 'chat_request', requestId, messages, maxTokens };
        if (typeof opts.temperature === 'number') payload.temperature = opts.temperature;
        if (typeof opts.top_p === 'number')       payload.top_p       = opts.top_p;
        if (typeof opts.seed === 'number')        payload.seed        = opts.seed;
        send(payload);
    });
}

// 2-shot: gen 2 candidates song song với temperature/seed khác, judge pick winner.
// Nếu chỉ 1 trong 2 thành công → dùng cái đó. Nếu cả 2 fail → throw.
async function requestChat2Shot(messages, maxTokens, judgeCtx) {
    const baseSeed = Math.floor(Math.random() * 1e9);
    const settled = await Promise.allSettled([
        requestChat(messages, maxTokens, { temperature: 0.85, seed: baseSeed }),
        requestChat(messages, maxTokens, { temperature: 1.05, seed: baseSeed + 7919 }),
    ]);
    const cands = [];
    for (let i = 0; i < settled.length; i++) {
        const s = settled[i];
        if (s.status === 'fulfilled' && s.value && s.value.trim()) {
            cands.push({ text: s.value.trim(), variant: i });
        }
    }
    if (cands.length === 0) {
        const err = settled.find(s => s.status === 'rejected')?.reason || new Error('all candidates failed');
        throw err;
    }
    if (cands.length === 1) {
        return { text: cands[0].text, debug: { single: true, variant: cands[0].variant } };
    }
    const pick = pickBest(cands, judgeCtx);
    const winner = cands[pick.idx];
    return { text: winner.text, debug: { picked: pick.idx, scores: pick.all, variant: winner.variant } };
}

// Called by app.js's WS handler when a {type:'chat'} arrives.
export function resolveChatMessage(msg) {
    const resolver = pendingChatRequests.get(msg.requestId);
    if (resolver) {
        pendingChatRequests.delete(msg.requestId);
        resolver(msg);
    }
}

// ── Importance gate for auto-commentary ─────────────────────────────

function moveImportance(ctx) {
    const r = ctx.result;
    if (!r) return 0;
    let score = 0;
    if (r.captured) {
        score += ({ q: 80, r: 50, b: 32, n: 32, p: 12 }[r.captured]) || 0;
    }
    const flags = r.flags || '';
    if (flags.includes('e')) score += 18;                           // en passant
    if (r.promotion) score += 70;                                    // promotion
    if (flags.includes('k') || flags.includes('q')) score += 40;     // castling
    if (state.game.inCheck()) score += 55;                           // delivers check
    if (state.plyCount <= 3) score += 28;                            // opening 1-3 plies
    const sinceLast = state.plyCount - state.lastChatPly;
    if (sinceLast >= 9) score += 25;
    return score;
}

function shouldChat(eventType, ctx) {
    if (['start', 'win', 'lose', 'draw', 'precognition'].includes(eventType)) return true;
    if (!state.currentPersona) return false;

    const sinceLast = state.plyCount - state.lastChatPly;

    // Theatrical kick-in: cho bot cơ hội chat ở turn "không có event lớn"
    // để có dịp off-topic. Không lệ thuộc moveImportance. Tỉ lệ thấp để
    // tránh huyên thuyên — đã giảm còn ~12-18% / turn theo chattiness.
    if (state.featureFlags?.theatrical && sinceLast >= 2) {
        const kickChance = (state.currentPersona.chattiness || 0.5) * 0.18;
        if (Math.random() < kickChance) return true;
    }

    let threshold = 80 - state.currentPersona.chattiness * 50;
    if (sinceLast <= 1) threshold += 35;
    else if (sinceLast === 2) threshold += 15;

    return moveImportance(ctx) >= threshold;
}

// ── Public chat triggers ───────────────────────────────────────────

export async function triggerPersonaChat(eventType, ctx = {}) {
    if (!state.currentPersona) return;
    if (state.mode !== 'pve') return;
    if (!shouldChat(eventType, ctx)) return;

    // Phase 1: bốc thể loại reaction trước khi build prompt.
    let reactionType = 'comment_move';
    let directive = null;
    let pickedTopicHint = null;
    if (state.featureFlags?.theatrical) {
        const moodLevel = currentMoodLevel();
        reactionType = pickReactionType(state.currentPersona, eventType, moodLevel);
        pickedTopicHint = pickTopicHint(state.currentPersona, reactionType);
        // Debug log để user verify trong devtools console.
        console.debug(`[persona] event=${eventType} mood=${moodLevel} reaction=${reactionType}` +
            (pickedTopicHint ? ` topic="${pickedTopicHint}"` : ''));
        if (reactionType === 'silent') {
            rememberReaction(reactionType);
            state.lastChatPly = state.plyCount;
            return;
        }
        directive = describeReaction(reactionType, pickedTopicHint, moodLevel);
    }

    state.lastChatPly = state.plyCount;
    const sys = buildSystemPrompt(state.currentPersona);
    const usr = buildEventPrompt(eventType, ctx, directive);
    const messages = [
        { role: 'system', content: sys },
        ...state.chatHistory,
        { role: 'user',   content: usr },
    ];
    state.chatBusy = true;
    refreshChatInputState();
    showTypingIndicator();
    try {
        const judgeCtx = {
            recentReplies: state.recentReplies || [],
            reactionType,
            moodLevel: currentMoodLevel(),
            personaSignatures: state.currentPersona?.signaturePhrases || [],
            topicHint: pickedTopicHint,
        };
        const { text, debug } = await requestChat2Shot(messages, 160, judgeCtx);
        if (debug?.scores) {
            const summary = debug.scores.map(s => `#${s.i}=${s.score}[${s.reasons.join(',') || 'ok'}]`).join(' ');
            console.debug(`[persona] 2-shot pick=#${debug.picked} | ${summary}`);
        } else if (debug?.single) {
            console.debug(`[persona] 2-shot single survivor variant=${debug.variant}`);
        }
        state.chatHistory.push({ role: 'user', content: usr });
        state.chatHistory.push({ role: 'assistant', content: text });
        if (state.chatHistory.length > MAX_CHAT_TURNS * 2) {
            state.chatHistory = state.chatHistory.slice(-MAX_CHAT_TURNS * 2);
        }
        state.recentReplies.push(text);
        if (state.recentReplies.length > 3) state.recentReplies = state.recentReplies.slice(-3);
        hideTypingIndicator();
        appendChatMsg('persona', text);
        rememberReaction(reactionType);
    } catch (err) {
        hideTypingIndicator();
        console.warn('persona chat failed:', err.message);
    } finally {
        state.chatBusy = false;
        refreshChatInputState();
    }
}

export async function sendUserChat(rawText) {
    const text = rawText.trim();
    if (!text) return;
    if (!state.currentPersona) {
        appendChatMsg('system', 'Hãy chọn một nhân vật ở mục “Trận đấu” trước khi chat.');
        return;
    }
    if (state.mode !== 'pve') {
        appendChatMsg('system', 'Chế độ hiện tại không có nhân vật để chat.');
        return;
    }
    if (state.chatBusy) return;

    appendChatMsg('user', text);

    const sys = buildSystemPromptForFreeChat(state.currentPersona);
    const messages = [
        { role: 'system', content: sys },
        ...state.chatHistory,
        { role: 'user', content: text },
    ];
    state.chatBusy = true;
    refreshChatInputState();
    setChatStatus('đang nghĩ…');
    showTypingIndicator();
    try {
        const judgeCtx = {
            recentReplies: state.recentReplies || [],
            reactionType: 'free_chat',
            moodLevel: currentMoodLevel(),
            personaSignatures: state.currentPersona?.signaturePhrases || [],
        };
        const { text: reply, debug } = await requestChat2Shot(messages, 220, judgeCtx);
        if (debug?.scores) {
            const summary = debug.scores.map(s => `#${s.i}=${s.score}[${s.reasons.join(',') || 'ok'}]`).join(' ');
            console.debug(`[persona free] 2-shot pick=#${debug.picked} | ${summary}`);
        }
        state.chatHistory.push({ role: 'user', content: text });
        state.chatHistory.push({ role: 'assistant', content: reply });
        if (state.chatHistory.length > MAX_CHAT_TURNS * 2) {
            state.chatHistory = state.chatHistory.slice(-MAX_CHAT_TURNS * 2);
        }
        state.recentReplies.push(reply);
        if (state.recentReplies.length > 3) state.recentReplies = state.recentReplies.slice(-3);
        hideTypingIndicator();
        appendChatMsg('persona', reply);
    } catch (err) {
        hideTypingIndicator();
        appendChatMsg('system', `(không nhận được phản hồi: ${err.message})`);
    } finally {
        state.chatBusy = false;
        setChatStatus('');
        refreshChatInputState();
    }
}
