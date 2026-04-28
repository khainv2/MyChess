// Headless self-play to evaluate persona/chat gating.
// Uses MyChessUCI (via the WS bridge) for both sides, and mirrors app.js
// importance/threshold logic to decide when to call the LLM.
//
// Usage:  node scripts/play-bot.js [personaId] [maxPlies]
const path = require('path');
const fs = require('fs');
const WebSocket = require('ws');
const { Chess } = require('chess.js');

const PERSONAS = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'public', 'personas.json'), 'utf8'));
const PERSONA_ID = process.argv[2] || 'ti-ham';
const MAX_PLIES  = parseInt(process.argv[3] || '60', 10);

const persona = PERSONAS.find(p => p.id === PERSONA_ID);
if (!persona) { console.error('persona not found:', PERSONA_ID); process.exit(2); }

console.log(`=== Self-play test ===`);
console.log(`Persona: ${persona.emoji} ${persona.name} (chatti ${persona.chattiness}, elo ${persona.elo})`);
console.log(`Max plies: ${MAX_PLIES}`);
console.log('---');

// Mirror app.js logic ---------------------------------------------------------

function eloConfig(elo) {
    if (elo <= 600)  return { movetimeMs: 50,   blunderProb: 0.50 };
    if (elo <= 800)  return { movetimeMs: 80,   blunderProb: 0.30 };
    if (elo <= 1000) return { movetimeMs: 120,  blunderProb: 0.18 };
    if (elo <= 1200) return { movetimeMs: 200,  blunderProb: 0.10 };
    if (elo <= 1400) return { movetimeMs: 400,  blunderProb: 0.05 };
    if (elo <= 1600) return { movetimeMs: 800,  blunderProb: 0.02 };
    if (elo <= 1800) return { movetimeMs: 1500, blunderProb: 0.005 };
    if (elo <= 2000) return { movetimeMs: 3000, blunderProb: 0    };
    return                  { movetimeMs: 6000, blunderProb: 0    };
}

const game = new Chess();
let plyCount = 0;
let lastChatPly = -10;
let chatCount = 0;
let blunderCount = 0;
const chatLog = []; // { ply, mover, san, importance, threshold, text, dt }

function moveImportance(result, inCheck) {
    let score = 0;
    if (result.captured) score += ({ q: 80, r: 50, b: 32, n: 32, p: 12 }[result.captured]) || 0;
    const flags = result.flags || '';
    if (flags.includes('e')) score += 18;
    if (result.promotion) score += 70;
    if (flags.includes('k') || flags.includes('q')) score += 40;
    if (inCheck) score += 55;
    if (plyCount <= 3) score += 28;
    const sinceLast = plyCount - lastChatPly;
    if (sinceLast >= 9) score += 25;
    return score;
}

const PIECE_VN = { p: 'Tốt', n: 'Mã', b: 'Tượng', r: 'Xe', q: 'Hậu', k: 'Vua' };
const pieceNameVN = (t) => PIECE_VN[t] || t;

function materialBalanceForPersona(personaColor) {
    const VALUES = { p: 1, n: 3, b: 3, r: 5, q: 9, k: 0 };
    let pSum = 0, oSum = 0;
    const board = game.board();
    for (let r = 0; r < 8; r++) for (let c = 0; c < 8; c++) {
        const sq = board[r][c]; if (!sq) continue;
        const v = VALUES[sq.type] || 0;
        if (sq.color === personaColor) pSum += v; else oSum += v;
    }
    return pSum - oSum;
}

function materialDescription(personaColor) {
    const d = materialBalanceForPersona(personaColor);
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
    const parts = [`đi quân ${pieceNameVN(result.piece)} (ký hiệu ${result.san})`];
    if (result.captured) parts.push(`ăn quân ${pieceNameVN(result.captured)} của đối thủ`);
    if (result.promotion) parts.push(`phong cấp thành ${pieceNameVN(result.promotion)}`);
    const flags = result.flags || '';
    if (flags.includes('k')) parts.push('nhập thành cánh Vua (O-O)');
    if (flags.includes('q')) parts.push('nhập thành cánh Hậu (O-O-O)');
    if (flags.includes('e')) parts.push('bắt tốt qua đường (en passant)');
    if (game.inCheck()) {
        parts.push(eventType === 'engine_move'
            ? 'nước này chiếu vua đối thủ'
            : 'nước này chiếu vua của bạn');
    }
    return parts.join(', ');
}

function thresholdNow() {
    let t = 80 - persona.chattiness * 50;
    const since = plyCount - lastChatPly;
    if (since <= 1) t += 35;
    else if (since === 2) t += 15;
    return t;
}

// WS plumbing -----------------------------------------------------------------

const ws = new WebSocket('ws://localhost:8080');
let pending = null;        // { kind:'best'|'chat', resolve, reject, requestId? }
let chatSeq = 0;

ws.on('open', () => console.log('[ws] open'));
ws.on('error', (e) => { console.error('[ws]', e.message); process.exit(1); });
ws.on('close', () => {});
ws.on('message', (raw) => {
    const msg = JSON.parse(raw.toString());
    if (msg.type === 'ready') { onReady(); return; }
    if (msg.type === 'bestmove' && pending && pending.kind === 'best') {
        const r = pending; pending = null; r.resolve(msg.uci);
    } else if (msg.type === 'chat' && pending && pending.kind === 'chat' && pending.requestId === msg.requestId) {
        const r = pending; pending = null;
        if (msg.ok) r.resolve(msg.text);
        else r.reject(new Error(msg.error || 'chat error'));
    } else if (msg.type === 'error') {
        console.warn('[ws err]', msg.msg);
    }
});

function send(obj) { ws.send(JSON.stringify(obj)); }

function getBestmove(movetimeMs) {
    return new Promise((resolve, reject) => {
        pending = { kind: 'best', resolve, reject };
        send({ type: 'go', movetimeMs });
        setTimeout(() => { if (pending && pending.kind === 'best') { pending = null; reject(new Error('bestmove timeout')); } }, movetimeMs + 8000);
    });
}

function getChat(systemPrompt, userPrompt) {
    return new Promise((resolve, reject) => {
        const requestId = `c${++chatSeq}`;
        pending = { kind: 'chat', resolve, reject, requestId };
        send({ type: 'chat_request', requestId, systemPrompt, userPrompt, maxTokens: 110 });
        setTimeout(() => { if (pending && pending.requestId === requestId) { pending = null; reject(new Error('chat timeout')); } }, 14000);
    });
}

// Main loop -------------------------------------------------------------------

async function onReady() {
    try {
        send({ type: 'newgame' });
        await runLoop();
        printSummary();
    } catch (e) {
        console.error('[loop]', e.message);
    } finally {
        ws.close();
        setTimeout(() => process.exit(0), 100);
    }
}

function buildSystemPrompt(p) {
    return [
        p.system_prompt,
        '',
        'Bối cảnh: bạn đang chơi cờ vua online (cầm Đen).',
        'Yêu cầu nghiêm ngặt: trả lời thật ngắn 1 câu (tối đa 2). Tiếng Việt thuần. Không emoji, không markdown, không xuống dòng.',
        'TUYỆT ĐỐI KHÔNG bịa thông tin về quân cờ. Chỉ nhắc đến những quân/sự kiện đã được nêu trong tin user. Nếu không chắc, nói chung chung ("lại đi rồi", "thấy ghê chưa", "không tệ") thay vì gọi sai tên quân.',
    ].join('\n');
}

function buildUserPrompt(eventType, ctx, personaColor) {
    const movesSan = game.history().slice(-6).join(' ');
    const matDesc = materialDescription(personaColor);
    if (eventType === 'start') return 'Ván cờ vừa bắt đầu. Hãy chào ngắn 1 câu theo tính cách của bạn.';
    if (eventType === 'engine_move') {
        return [
            `Bạn vừa ${describeMove(ctx.result, eventType)}.`,
            `Tình thế tổng quan: ${matDesc}.`,
            `Lịch sử gần đây: ${movesSan}.`,
            `Hãy nói 1 câu thật ngắn theo đúng tính cách. Không phân tích chuyên môn dài dòng.`,
        ].join(' ');
    }
    if (eventType === 'user_move') {
        return [
            `Đối thủ vừa ${describeMove(ctx.result, eventType)}.`,
            `Tình thế tổng quan: ${matDesc}.`,
            `Lịch sử gần đây: ${movesSan}.`,
            `Hãy phản ứng 1 câu ngắn theo đúng tính cách.`,
        ].join(' ');
    }
    return 'Hãy nói 1 câu phù hợp tình huống.';
}

async function runLoop() {
    // Persona side: black (so engine = white starts).
    const personaColor = 'b';
    const cfg = eloConfig(persona.elo);

    // Start chat (always fires).
    try {
        const t0 = Date.now();
        const text = await getChat(buildSystemPrompt(persona), buildUserPrompt('start', {}, personaColor));
        chatCount++;
        lastChatPly = plyCount;
        const dt = Date.now() - t0;
        chatLog.push({ ply: 0, mover: '-', san: '(start)', importance: 999, threshold: 0, text, dt });
        console.log(`[ply 0] (start) chat ${dt}ms: "${text}"`);
    } catch (e) { console.warn('[start chat fail]', e.message); }

    while (plyCount < MAX_PLIES && !game.isGameOver()) {
        const sideToMove = game.turn();
        const isPersona = (sideToMove === personaColor);

        let uci;
        try { uci = await getBestmove(cfg.movetimeMs); }
        catch (e) { console.warn('[engine]', e.message); break; }
        if (!uci) { console.log('[engine] none — bailing'); break; }

        // Blunder injection on persona side.
        if (isPersona && cfg.blunderProb > 0 && Math.random() < cfg.blunderProb) {
            const legal = game.moves({ verbose: true });
            if (legal.length > 1) {
                const pick = legal[Math.floor(Math.random() * legal.length)];
                uci = pick.from + pick.to + (pick.promotion || '');
                blunderCount++;
            }
        }

        // Apply on local board.
        const from = uci.slice(0, 2), to = uci.slice(2, 4);
        const promo = uci.length >= 5 ? uci[4] : undefined;
        let result;
        try { result = game.move({ from, to, promotion: promo }); }
        catch (_) { result = null; }
        if (!result) { console.log('[apply] illegal:', uci); break; }
        plyCount++;
        const inCheck = game.inCheck();
        const importance = moveImportance(result, inCheck);
        const threshold = thresholdNow();
        const eventType = isPersona ? 'engine_move' : 'user_move';
        const willChat = importance >= threshold;
        const sideTag = isPersona ? 'PERSONA' : 'opp    ';
        const flagInfo = [
            result.captured ? `x${result.captured}` : null,
            inCheck ? '+' : null,
            result.promotion ? `=${result.promotion.toUpperCase()}` : null,
            (result.flags || '').includes('k') ? 'O-O' : null,
            (result.flags || '').includes('q') ? 'O-O-O' : null,
        ].filter(Boolean).join(' ');
        const head = `[ply ${String(plyCount).padStart(2)}] ${sideTag} ${result.san.padEnd(7)} imp=${String(importance).padStart(3)} th=${String(threshold).padStart(3)} ${flagInfo}`;

        // Persona reacts on BOTH sides' moves (mirrors real app.js behavior).
        if (willChat) {
            try {
                const t0 = Date.now();
                const text = await getChat(buildSystemPrompt(persona), buildUserPrompt(eventType, { result }, personaColor));
                const dt = Date.now() - t0;
                chatCount++;
                lastChatPly = plyCount;
                chatLog.push({ ply: plyCount, mover: sideTag, san: result.san, importance, threshold, text, dt });
                console.log(`${head}  💬 ${dt}ms: "${text}"`);
            } catch (e) {
                console.log(`${head}  ⚠ chat fail: ${e.message}`);
            }
        } else {
            console.log(head);
        }

        // Sync engine via FEN (avoids tracking moves array).
        send({ type: 'position', fen: game.fen() });
    }

    // End-of-game milestone (always fires if game ended naturally).
    if (game.isGameOver()) {
        let evt = 'draw';
        if (game.isCheckmate()) {
            const losing = game.turn();
            evt = (losing === 'b') ? 'win' : 'lose'; // persona = black
        }
        try {
            const t0 = Date.now();
            const text = await getChat(buildSystemPrompt(persona),
                evt === 'win' ? 'Bạn vừa thắng. Nói 1 câu thật ngắn theo tính cách.' :
                evt === 'lose' ? 'Bạn vừa thua. Nói 1 câu thật ngắn.' :
                'Ván cờ hòa. Nói 1 câu thật ngắn.');
            const dt = Date.now() - t0;
            chatCount++;
            chatLog.push({ ply: plyCount, mover: '-', san: `(${evt})`, importance: 999, threshold: 0, text, dt });
            console.log(`[ply ${plyCount}] (${evt}) chat ${dt}ms: "${text}"`);
        } catch (e) { console.warn('[end chat fail]', e.message); }
    }
}

function printSummary() {
    console.log('\n=== SUMMARY ===');
    console.log(`Total plies played:     ${plyCount}`);
    console.log(`Chat triggers:          ${chatCount}`);
    console.log(`Persona-side blunders:  ${blunderCount}`);
    console.log(`Game over:              ${game.isGameOver()}  (${game.isCheckmate() ? 'checkmate' : game.isStalemate() ? 'stalemate' : game.isDraw() ? 'draw' : 'unfinished'})`);
    console.log(`Final FEN:              ${game.fen()}`);
    if (chatLog.length) {
        const avgDt = Math.round(chatLog.reduce((s, x) => s + (x.dt || 0), 0) / chatLog.length);
        console.log(`Avg LLM latency:        ${avgDt}ms`);
    }
}
