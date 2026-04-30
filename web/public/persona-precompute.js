// Phase 3 — Engine pre-compute.
// Khi tới lượt user, gửi 1 request precompute lên server (engine thứ 2 song
// song với engine chính). Cache top-1 nước cho phía user; sau khi user đi
// thực sự thì compare để biết user đi đúng/sai best move (qualitative).
//
// Engine MyChess không hỗ trợ multipv → chỉ có top-1. Đủ dùng cho:
//   1. Tiên tri: "tớ đoán cậu sẽ đi ..." (không nói chính xác nước)
//   2. So sánh hậu kiểm: "ơ tớ tưởng cậu sẽ đi khác" / "đúng rồi đó"

import { state, send } from './state.js';
import { Chess } from './vendor/chess.js';

let preReqSeq = 0;

function uciToSan(fen, uci) {
    if (!uci) return null;
    try {
        const g = new Chess(fen);
        const from = uci.slice(0, 2), to = uci.slice(2, 4);
        const promotion = uci.length > 4 ? uci[4] : undefined;
        const m = g.move({ from, to, promotion });
        return m ? m.san : null;
    } catch (_) { return null; }
}

// Persona elo ≤1500 → mặc định ON. Lớn hơn → OFF (cao thủ không cần engine
// gợi ý, dễ lộ best move). User vẫn override được qua feature flag global.
export function shouldPrecomputeForPersona(persona) {
    if (!state.featureFlags?.precompute) return false;
    if (!persona) return false;
    return (persona.elo || 9999) <= 1500;
}

export function resetPrecompute() {
    state.preComputed = {
        forFen: null,
        uci: null,
        san: null,
        scoreCp: null,
        computedAt: 0,
    };
}

// Gửi request lên server. Khi response về sẽ được handlePrecomputeResponse.
export function requestPrecompute() {
    if (!shouldPrecomputeForPersona(state.currentPersona)) return;
    if (state.gameOver) return;
    const fen = state.game.fen();
    const id = `pre-${++preReqSeq}`;
    state.preComputed = { forFen: fen, uci: null, san: null, scoreCp: null, computedAt: 0, requestId: id };
    send({ type: 'precompute_request', requestId: id, fen, movetimeMs: 350 });
}

// Gọi từ app.js WS handler khi nhận {type:'precompute', ...}
export function handlePrecomputeResponse(msg) {
    const pc = state.preComputed;
    if (!pc || pc.requestId !== msg.requestId) return;  // outdated
    if (!msg.ok || !msg.uci) {
        pc.uci = null; pc.san = null; pc.scoreCp = null;
        return;
    }
    pc.uci = msg.uci;
    pc.san = uciToSan(pc.forFen, msg.uci);
    if (msg.score && msg.score.kind === 'cp') pc.scoreCp = msg.score.value;
    else if (msg.score && msg.score.kind === 'mate') pc.scoreCp = msg.score.value > 0 ? 10000 : -10000;
    else pc.scoreCp = null;
    pc.computedAt = Date.now();
    console.debug(`[precompute] top1 for user: ${pc.san || pc.uci} (cp=${pc.scoreCp})`);
}

// Sau khi user đi xong: so sánh nước user vs precomputed top-1.
// Trả về 'matched' | 'differed' | 'unknown'.
export function compareUserMove(uciPlayed, fenBeforeUserMove) {
    const pc = state.preComputed;
    if (!pc || !pc.uci || pc.forFen !== fenBeforeUserMove) return { quality: 'unknown' };
    if (uciPlayed === pc.uci) return { quality: 'matched', expectedSan: pc.san };
    return { quality: 'differed', expectedSan: pc.san };
}

// Trả về hint text để inject prompt khi user nghĩ lâu (precognition event).
// Cố tình KHÔNG để lộ chính xác nước cờ — chỉ ám chỉ chung chung.
export function getPrecognitionHint() {
    const pc = state.preComputed;
    if (!pc || !pc.uci || !pc.san) return null;
    // Phân loại chung chung theo score để bot nói có cơ sở
    const cp = pc.scoreCp;
    let evalDesc;
    if (cp == null)            evalDesc = 'thế cờ khá cân';
    else if (cp >=  300)       evalDesc = 'thế cờ đang nghiêng hẳn về phía đối thủ — khó cho bạn';
    else if (cp >=  100)       evalDesc = 'thế cờ đang có lợi cho đối thủ';
    else if (cp >=  -100)      evalDesc = 'thế cờ khá cân, ai cũng có cơ hội';
    else if (cp >=  -300)      evalDesc = 'thế cờ đang có lợi cho bạn';
    else                       evalDesc = 'thế cờ rất tốt cho bạn — đối thủ khó gỡ';
    // Phân loại quân theo SAN ký hiệu đầu (K/Q/R/B/N/none=tốt)
    const piece = pc.san[0];
    let pieceHint;
    if (piece === 'N')      pieceHint = 'có vẻ sẽ đẩy Mã ra';
    else if (piece === 'B') pieceHint = 'có vẻ sẽ đẩy Tượng ra';
    else if (piece === 'R') pieceHint = 'có vẻ sẽ đi Xe';
    else if (piece === 'Q') pieceHint = 'có vẻ sẽ điều Hậu';
    else if (piece === 'K') pieceHint = 'có vẻ sẽ đi Vua (an toàn nó)';
    else if (pc.san.includes('O-O-O')) pieceHint = 'có vẻ sẽ nhập thành cánh Hậu';
    else if (pc.san.includes('O-O'))   pieceHint = 'có vẻ sẽ nhập thành';
    else                    pieceHint = 'có vẻ sẽ đẩy Tốt';
    return { evalDesc, pieceHint, expectedSan: pc.san };
}
