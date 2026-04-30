// Heuristic local judge — chấm điểm 1 reply persona (free + auto-commentary).
// Cơ chế "2-shot + judge": gen 2 candidate song song với temperature/seed khác,
// chấm bằng heuristic này, pick câu điểm cao hơn.
//
// Càng cao càng tốt. Base = 100, trừ điểm cho mọi vi phạm. KHÔNG gọi LLM,
// chạy local instant — phù hợp pipeline real-time.
//
// Input: text (string) + ctx { recentReplies, reactionType, moodLevel }.
// Output: { score: number, reasons: string[] } — caller dùng .score để compare.

// SAN: piece + opt(disamb) + opt(x) + dest + opt(promo) + opt(check)
//      hoặc castling O-O / O-O-O
//      hoặc UCI thuần e2e4 (4 chữ liền). KHÔNG match "ô e4" hay địa chỉ.
const SAN_RE = /(?:^|\s)([NBRQK][a-h]?[1-8]?x?[a-h][1-8](?:=[NBRQ])?[+#]?|O-O(?:-O)?|[a-h][1-8][a-h][1-8])(?:\s|$|[.,!?])/;

// Tên quân tiếng Anh — bot thi thoảng vẫn xổ.
const EN_PIECE_RE = /\b(knights?|bishops?|rooks?|queens?|kings?|pawns?)\b/i;

// Mở câu bằng cụm dập khuôn (đã ban trong prompt nhưng bot vẫn lén).
const FORBIDDEN_OPENERS = [
    /^ờ\s+t[ưu]/i,         // "ờ tớ" / "ờ tự"
    /^ơ\s+t[ưu]/i,         // "ơ tớ"
    /^ờ\s+nước/i,
    /^ơ\s+nước/i,
];

// Token chứng tỏ đang nói cờ — cấm xuất hiện khi reactionType là off-topic.
const CHESS_TOKEN_RE = /(cờ\s*vua|nước\s*cờ|quân\s*cờ|bàn\s*cờ|chiếu\s*vua|chiếu\s*tướng|chiếu\s*hết|nhập\s*thành|phong\s*cấp|ăn\s*tốt|ăn\s*mã|ăn\s*tượng|ăn\s*hậu|ăn\s*xe|ăn\s*vua|nước\s*này|nước\s*đi)/i;

// Bot copy nguyên văn label mood vào câu (regression của Round 2).
const MOOD_LITERAL = ['hoảng loạn', 'đắc thắng', 'vênh váo', 'phấn khởi', 'căng thẳng', 'mỏi mệt', 'khoái chí', 'lo lắng'];

// Reaction type phải hoàn toàn off-topic, không được nhắc cờ.
const OFF_REACTIONS = new Set(['off_topic_personal', 'meta', 'random_observation', 'question_back']);

function normalize(s) {
    return (s || '')
        .toLowerCase()
        .replace(/[.,!?…"'":;()\[\]{}]/g, ' ')
        .replace(/\s+/g, ' ')
        .trim();
}

function tokenSet(s) {
    return new Set(normalize(s).split(' ').filter(t => t.length > 1));
}

function jaccardOverlap(a, b) {
    const A = tokenSet(a), B = tokenSet(b);
    if (A.size === 0 || B.size === 0) return 0;
    let inter = 0;
    for (const t of A) if (B.has(t)) inter++;
    const uni = A.size + B.size - inter;
    return uni > 0 ? inter / uni : 0;
}

function firstNgram(s, n = 3) {
    return normalize(s).split(' ').slice(0, n).join(' ');
}

function bigrams(s) {
    const ts = normalize(s).split(' ').filter(t => t.length > 0);
    const out = [];
    for (let i = 0; i < ts.length - 1; i++) out.push(ts[i] + ' ' + ts[i + 1]);
    return out;
}

// Signature interjections / catchphrases — phạt khi lặp giữa các reply liên tiếp.
// Đây là những âm đặc trưng persona; bot có xu hướng nhồi vào MỌI câu → cringe.
const SIGNATURE_TOKENS = [
    'ehe', 'ehee', 'ehehe',
    'hì', 'hì hì', 'hihi', 'hihihi',
    'hà hà', 'haha', 'hahaha',
    'haizz', 'haiz',
    'hức', 'hứt',
    'oách xà lách',
    'oa',
    'ơ kìa', 'trời ơi',
    'chết thật',
    'đỡ chứ',
    'non lắm', 'gà thế',
    'vãi cả', 'đỉnh',
    'chà chà', 'hừm',
];

export function scoreReply(text, ctx = {}) {
    const {
        recentReplies = [],
        reactionType = 'comment_move',
        moodLevel = 'neutral',
        personaSignatures = [],
        topicHint = null,
    } = ctx;
    if (!text || typeof text !== 'string') {
        return { score: -1000, reasons: ['empty'] };
    }
    const trimmed = text.trim();
    if (trimmed.length === 0) {
        return { score: -1000, reasons: ['empty'] };
    }

    let score = 100;
    const reasons = [];
    const lower = trimmed.toLowerCase();

    // ── Length ────────────────────────────────────────
    if (trimmed.length < 8)   { score -= 35; reasons.push('too_short'); }
    if (trimmed.length > 240) { score -= 25; reasons.push('long'); }
    if (trimmed.length > 320) { score -= 30; reasons.push('way_too_long'); }

    // ── Notation / English leak ───────────────────────
    if (SAN_RE.test(' ' + trimmed + ' ')) { score -= 60; reasons.push('san_leak'); }
    if (EN_PIECE_RE.test(trimmed))         { score -= 45; reasons.push('english_piece'); }

    // ── Markup / emoji / wrap quotes ──────────────────
    if (/[*_`#]{1,}/.test(trimmed))                         { score -= 25; reasons.push('markdown'); }
    if (/[\u{1F300}-\u{1FAFF}]|[\u{2600}-\u{27BF}]/u.test(trimmed)) { score -= 20; reasons.push('emoji'); }
    if (/^["'"'"]/.test(trimmed) || /["'"'"]$/.test(trimmed))      { score -= 12; reasons.push('wrap_quotes'); }

    // ── Forbidden openers ─────────────────────────────
    for (const re of FORBIDDEN_OPENERS) {
        if (re.test(trimmed)) { score -= 30; reasons.push('bad_opener'); break; }
    }

    // ── Off-topic family must NOT mention chess ───────
    if (OFF_REACTIONS.has(reactionType) && CHESS_TOKEN_RE.test(lower)) {
        score -= 55;
        reasons.push('off_topic_with_chess');
    }

    // ── Mood label literal copy ───────────────────────
    for (const lit of MOOD_LITERAL) {
        if (lower.includes(lit)) { score -= 25; reasons.push('mood_literal'); break; }
    }

    // ── Anti-repeat vs recentReplies ──────────────────
    let maxOv = 0, sameStart = false;
    const head = firstNgram(trimmed, 3);
    for (const r of recentReplies) {
        const ov = jaccardOverlap(trimmed, r);
        if (ov > maxOv) maxOv = ov;
        if (head.length > 0 && firstNgram(r, 3) === head) sameStart = true;
    }
    if (maxOv > 0.5)        { score -= 55; reasons.push('high_overlap'); }
    else if (maxOv > 0.35)  { score -= 28; reasons.push('mid_overlap'); }
    if (sameStart)          { score -= 22; reasons.push('same_opener_as_recent'); }

    // ── Catchphrase / signature overuse ───────────────
    // Đây là điểm mù lớn nhất khi LLM nhồi catchphrase vào mỗi câu (ehe, oách
    // xà lách, hì hì, haizz...). Nếu signature đã xuất hiện ≥1 lần trong recent
    // và lại xuất hiện trong câu mới → penalty nặng để 2-shot pick câu sạch hơn.
    // Tổng hợp: signature global + signature riêng của persona.
    const allSignatures = [...SIGNATURE_TOKENS, ...personaSignatures.map(s => s.toLowerCase())];
    let sigHits = 0;
    for (const sig of allSignatures) {
        if (!sig || !lower.includes(sig)) continue;
        for (const r of recentReplies) {
            if ((r || '').toLowerCase().includes(sig)) { sigHits++; break; }
        }
    }
    if (sigHits >= 2)       { score -= 50; reasons.push('signature_overuse_heavy'); }
    else if (sigHits >= 1)  { score -= 28; reasons.push('signature_overuse'); }

    // ── Signature lặp TRONG cùng 1 câu (vd "chú em ... chú em ...") ──
    // Catch trường hợp bot ghép cùng signature 2 lần trong CÙNG reply.
    let inSentenceDup = 0;
    for (const sig of allSignatures) {
        if (!sig) continue;
        let count = 0, idx = 0;
        while ((idx = lower.indexOf(sig, idx)) !== -1) { count++; idx += sig.length; }
        if (count >= 2) inSentenceDup++;
    }
    if (inSentenceDup >= 1) { score -= 30; reasons.push('signature_dup_in_sentence'); }

    // ── Copy nguyên văn topic hint ──
    // Nếu reply chứa cụm 4+ từ liên tiếp y nguyên topicHint → bot đã copy, fail.
    if (topicHint) {
        const hintLower = (topicHint || '').toLowerCase();
        const hintWords = hintLower.split(/\s+/).filter(w => w.length > 1);
        if (hintWords.length >= 4) {
            // Thử các cụm 4-gram từ topicHint xem có trong reply không
            let matched4gram = false;
            for (let i = 0; i + 4 <= hintWords.length; i++) {
                const ng = hintWords.slice(i, i + 4).join(' ');
                if (lower.includes(ng)) { matched4gram = true; break; }
            }
            if (matched4gram) { score -= 60; reasons.push('topic_hint_copied'); }
        }
        // Jaccard overlap với topic hint cũng phạt
        const ov = jaccardOverlap(trimmed, topicHint);
        if (ov > 0.55)        { score -= 45; reasons.push('topic_hint_high_overlap'); }
        else if (ov > 0.4)    { score -= 22; reasons.push('topic_hint_overlap'); }
    }

    // ── Bigram overlap với recent (catch khi cùng cụm 2-từ lặp lại) ──
    const myBg = new Set(bigrams(trimmed));
    let bgHits = 0;
    for (const r of recentReplies) {
        for (const bg of bigrams(r)) if (myBg.has(bg)) bgHits++;
    }
    if (bgHits >= 4)        { score -= 45; reasons.push('bigram_repeat_heavy'); }
    else if (bgHits >= 2)   { score -= 22; reasons.push('bigram_repeat'); }

    // ── Stuffing nhiều catchphrase trong CÙNG 1 câu ──
    // Bot đôi khi ghép 3-4 catchphrase trong 1 câu (vd "trời ơi ehe oách xà
    // lách thôi mà"). Đếm và phạt.
    let stuffCount = 0;
    for (const sig of SIGNATURE_TOKENS) if (lower.includes(sig)) stuffCount++;
    if (stuffCount >= 3)    { score -= 35; reasons.push('catchphrase_stuffed'); }

    // ── Repetition within text ────────────────────────
    const words = normalize(trimmed).split(' ');
    const wfreq = {};
    for (const w of words) if (w.length > 2) wfreq[w] = (wfreq[w] || 0) + 1;
    const maxFreq = Math.max(0, ...Object.values(wfreq));
    if (maxFreq >= 4) { score -= 25; reasons.push('repeat_word'); }

    // ── Sentence count ────────────────────────────────
    const sentences = trimmed.split(/[.!?…]+/).filter(s => s.trim().length > 0);
    if (sentences.length > 3) { score -= 15; reasons.push('too_many_sentences'); }

    // ── Detect câu cụt ────────────────────────────────
    // Triệu chứng: max_tokens cắt giữa câu — kết thúc bằng giới từ/số/từ ngắn,
    // không có dấu kết câu, hoặc lửng. Vd "Phòng trọ tớ ở Cầu Giấy ba" hay
    // "Thích sưu tập thẻ Pokemon nhất là".
    const lastChar = trimmed.slice(-1);
    const endsWithPunct = /[.!?…)\]"'"'"]/.test(lastChar);
    const tail = trimmed.split(/\s+/).slice(-2).join(' ').toLowerCase();
    const TRAILING_INCOMPLETE = /\b(là|của|với|và|hay|như|cho|về|từ|tại|theo|trên|dưới|trong|ngoài|nhất|hơn|một|hai|ba|bốn|năm)$/;
    const lastWord = trimmed.split(/\s+/).pop() || '';
    if (!endsWithPunct && trimmed.length > 30) {
        if (TRAILING_INCOMPLETE.test(tail) || lastWord.length <= 2) {
            score -= 40; reasons.push('truncated_sentence');
        } else {
            // Không có dấu kết, không phải từ "đáng ngờ" — nhẹ thôi.
            score -= 8; reasons.push('no_end_punct');
        }
    }

    // ── Bonus: có dấu thân mật (xưng hô) cho free-chat tự nhiên ──
    // Persona Bắc dùng "tớ/cậu/em/anh/chị" nhiều — câu nào có là dấu hiệu in-character.
    if (/\b(t[ớo]|c[ậa]u|em|anh|ch[ịi]|m[ìi]nh|b[ạa]n)\b/i.test(lower)) {
        score += 4;
    }

    // ── Bonus nhẹ: câu ngắn-vừa (40-160 chars) là sweet spot cho 1-2 câu ──
    if (trimmed.length >= 25 && trimmed.length <= 160) {
        score += 6;
    }

    return { score, reasons };
}

// ── Helper: pick winner từ array candidates ──
// Mỗi candidate: { text: string }. Trả về index của winner (-1 nếu rỗng).
// Ties broken bởi index trước (stable).
export function pickBest(candidates, ctx) {
    if (!Array.isArray(candidates) || candidates.length === 0) return -1;
    let bestIdx = -1, bestScore = -Infinity, bestReasons = null;
    const scoredAll = [];
    for (let i = 0; i < candidates.length; i++) {
        const c = candidates[i];
        if (!c || !c.text) continue;
        const s = scoreReply(c.text, ctx);
        scoredAll.push({ i, score: s.score, reasons: s.reasons });
        if (s.score > bestScore) {
            bestScore = s.score;
            bestIdx = i;
            bestReasons = s.reasons;
        }
    }
    return { idx: bestIdx, score: bestScore, reasons: bestReasons, all: scoredAll };
}
