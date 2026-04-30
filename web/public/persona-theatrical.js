// Theatrical choice + topic hint cho auto-commentary.
// Mục đích: bot không cứ phải bình luận nước cờ mỗi lượt — có lúc bâng quơ
// đời tư, có lúc trash-talk vô thưởng vô phạt, có lúc im lặng.
//
// pickReactionType() → trả về 1 trong 7 thể loại reaction (weighted random,
// có anti-repeat). Caller gắn vào prompt; nếu 'silent' thì skip LLM call.

import { state } from './state.js';

// Trọng số mặc định khi persona không khai báo `reactionWeights`.
const DEFAULT_WEIGHTS = {
    comment_move:       35,
    bantering:          20,
    off_topic_personal: 15,
    meta:               10,
    question_back:       8,
    random_observation:  7,
    silent:              5,
};

// Event nào bot bắt buộc phải bình luận đúng tình huống (không random off-topic).
const FORCE_COMMENT = new Set(['start', 'win', 'lose', 'draw', 'precognition']);

const RECENT_BUF_SIZE = 4;

function pickWeighted(weights) {
    let total = 0;
    for (const k in weights) total += Math.max(0, weights[k] || 0);
    if (total <= 0) return 'comment_move';
    let r = Math.random() * total;
    for (const k in weights) {
        const w = Math.max(0, weights[k] || 0);
        if (r < w) return k;
        r -= w;
    }
    return 'comment_move';
}

// Hệ số scale weights theo mood. Mỗi mood điều chỉnh tendency thoại của bot.
const MOOD_MULTIPLIERS = {
    panic:    { silent: 2.0, bantering: 0.3, off_topic_personal: 0.6 },
    tense:    { silent: 1.5, bantering: 0.5 },
    gloating: { bantering: 2.5, comment_move: 1.3, silent: 0.4 },
    happy:    { bantering: 1.5, off_topic_personal: 1.2, silent: 0.7 },
    bored:    { off_topic_personal: 1.8, random_observation: 1.6, comment_move: 0.6 },
    neutral:  {},
};

// Hard-ban: mood X tuyệt đối không trigger reaction Y (vô lý logic).
// VD panic mà hỏi "cậu thích ăn gì?" hay gloating mà hỏi "trà sữa Phúc Long".
const MOOD_HARD_BAN = {
    panic:    new Set(['question_back', 'random_observation', 'off_topic_personal', 'meta']),
    tense:    new Set(['question_back']),
    gloating: new Set(['question_back', 'random_observation']),
    bored:    new Set([]),  // bored cho phép tất cả — đó là điểm hay của bored
    happy:    new Set([]),
    neutral:  new Set([]),
};

export function pickReactionType(persona, eventType, moodLevel = 'neutral') {
    if (FORCE_COMMENT.has(eventType)) return 'comment_move';
    const base = (persona && persona.reactionWeights) || DEFAULT_WEIGHTS;
    const weights = { ...base };

    // Mood scaling
    const mult = MOOD_MULTIPLIERS[moodLevel] || {};
    for (const k of Object.keys(weights)) {
        if (typeof mult[k] === 'number') {
            weights[k] = Math.max(0, Math.floor(weights[k] * mult[k]));
        }
    }
    // Hard ban — set 0 cho reaction không hợp mood (logic vô lý).
    const ban = MOOD_HARD_BAN[moodLevel] || new Set();
    for (const k of ban) weights[k] = 0;

    const recent = state.recentReactions || [];
    const last2 = recent.slice(-2);
    // Anti-repeat: nếu 2 turn vừa rồi đều comment_move → giảm 50%, ép đa dạng
    if (last2.length === 2 && last2.every(x => x === 'comment_move')) {
        weights.comment_move = Math.floor((weights.comment_move || 0) * 0.5);
    }
    // Anti-repeat off-topic family: nếu 2/3 turn vừa rồi đều off-topic
    // (off_topic_personal / meta / random_observation) → cấm cả family lần này.
    // Tránh trường hợp bot huyên thuyên 3-4 câu liên tiếp toàn kể chuyện đời.
    const OFF_FAMILY = ['off_topic_personal', 'meta', 'random_observation'];
    const recentOffCount = recent.slice(-3).filter(x => OFF_FAMILY.includes(x)).length;
    if (recentOffCount >= 2) {
        for (const k of OFF_FAMILY) weights[k] = 0;
    } else if (recent.length && OFF_FAMILY.includes(recent[recent.length - 1])) {
        // Vừa off-topic xong → giảm 60% lần này
        for (const k of OFF_FAMILY) weights[k] = Math.floor((weights[k] || 0) * 0.4);
    }
    // Cap silent: không 2 lần silent liên tiếp
    if (recent.length && recent[recent.length - 1] === 'silent') {
        weights.silent = 0;
    }
    // Cap silent xa hơn: tối đa 1 lần silent / 5 turn
    const silentInWindow = recent.filter(x => x === 'silent').length;
    if (silentInWindow >= 1) weights.silent = 0;

    return pickWeighted(weights);
}

// Mỗi reaction type kéo về 1 hoặc nhiều category trong topics{}.
const TOPIC_BY_CHOICE = {
    off_topic_personal: ['personal_life', 'complaints', 'boasts'],
    random_observation: ['small_talk'],
    meta:               ['hobbies', 'personal_life'],
};

export function pickTopicHint(persona, reactionType) {
    const cats = TOPIC_BY_CHOICE[reactionType];
    if (!cats || !persona || !persona.topics) return null;
    const cat = cats[Math.floor(Math.random() * cats.length)];
    const list = persona.topics[cat];
    if (!Array.isArray(list) || list.length === 0) return null;
    // Anti-repeat: tránh pick lại topic đã dùng trong 4 lần gần nhất.
    if (!state.recentTopics) state.recentTopics = [];
    const recent = new Set(state.recentTopics);
    const candidates = list.filter(t => !recent.has(t));
    const pool = candidates.length > 0 ? candidates : list;
    const picked = pool[Math.floor(Math.random() * pool.length)];
    state.recentTopics.push(picked);
    if (state.recentTopics.length > 4) state.recentTopics = state.recentTopics.slice(-4);
    return picked;
}

// Common ban applied tới mọi reaction off-topic. Bot rất hay nhồi cờ vào
// câu off-topic (ví dụ: "đi Mã ấy hả, vừa rớt môn giải tích"). Phải cấm
// thẳng tay — nếu đã off-topic thì 0% mention cờ.
const OFF_TOPIC_BAN = 'TUYỆT ĐỐI KHÔNG nhắc đến cờ vua, nước cờ, quân cờ, bàn cờ, ô bàn cờ, kết quả thắng/thua, hay bất kỳ từ nào liên quan trận đấu trong câu này.';

// Mood-aware tone constraint cho off-topic. Bot hay tự gen "chuyện buồn nặng"
// kể cả khi mood neutral/happy → cringe. Phải sync.
const POSITIVE_MOODS = new Set(['neutral', 'happy', 'gloating']);
const HEAVY_TOPIC_BAN = 'KHÔNG nói chuyện buồn nặng/mất mát/đại tang/người thân qua đời/ốm nặng — chỉ chuyện đời thường, vụn vặt, vui hoặc trung tính (đồ ăn, học hành, công việc, bạn bè, lười biếng, rảnh rỗi…).';

// Topic hint từng pass nguyên văn full sentence → LLM cứ copy y chang.
// Đổi sang gợi ý dạng "ý tưởng/keyword" để LLM phải tự đặt câu mới.
// Cắt hint còn 4-7 từ key (loại bỏ phần đầu/cuối), bỏ động từ chia.
function distillTopic(hint) {
    if (!hint) return '';
    // Cắt còn ≤7 từ đầu, loại bỏ chủ ngữ "tớ/tao/em/cô/cụ/anh tớ/mẹ tớ"
    let s = hint.replace(/^(tớ|tao|em|cô|cụ|anh|chị|mẹ|bố|bà|ông)\s+(tớ|tao)?\s*/i, '');
    s = s.replace(/^(hôm nay|hôm qua|tối qua|sáng nay|hôm trước|tối nào|buổi chiều|cuối tuần)\s+/i, '');
    const words = s.split(/\s+/).slice(0, 7);
    return words.join(' ');
}

function topicLine(topicHint) {
    if (!topicHint) return '';
    const seed = distillTopic(topicHint);
    return ` Ý tưởng nội dung (chỉ là gợi ý concept, KHÔNG phải template, KHÔNG được lặp nguyên văn): ${seed}. PHẢI viết câu HOÀN TOÀN MỚI theo giọng persona, dùng từ ngữ khác hẳn gợi ý — chỉ giữ lại ý tứ, KHÔNG giữ cấu trúc/cụm từ của gợi ý.`;
}

function moodToneLine(moodLevel) {
    return POSITIVE_MOODS.has(moodLevel) ? ` ${HEAVY_TOPIC_BAN}` : '';
}

export function describeReaction(reactionType, topicHint, moodLevel = 'neutral') {
    const moodTone = moodToneLine(moodLevel);
    switch (reactionType) {
        case 'comment_move':
            return 'Hãy bình luận ngắn về nước cờ vừa xảy ra theo đúng tính cách (chê/khen/trêu/dửng dưng). KHÔNG nói formal kiểu MC khai mạc/chúc tụng tournament ("mong chúng ta", "ván đấu sòng phẳng", "chúc may mắn"). Nói như đời thường — bạn bè/người quen đang tán dóc, không phải lễ khai mạc.';
        case 'bantering':
            return 'Lần này HÃY: nói 1 câu trash-talk hoặc đùa vô thưởng vô phạt theo đúng giọng persona, có thể liên quan thế cờ chung chung nhưng KHÔNG phân tích chi tiết nước cờ vừa rồi.';
        case 'off_topic_personal':
            return `Lần này HÃY: nói 1 câu bâng quơ về đời sống cá nhân của bạn, đúng giọng persona. ${OFF_TOPIC_BAN}${moodTone}${topicLine(topicHint)}`;
        case 'meta':
            return `Lần này HÃY: tự nói 1 câu về bản thân/cảm xúc/sở thích bạn đang nghĩ. ${OFF_TOPIC_BAN}${moodTone}${topicLine(topicHint)}`;
        case 'question_back':
            return `Lần này HÃY: hỏi ngược NGƯỜI CHƠI (đối thủ ngồi đối diện) 1 câu về cuộc sống của họ, đúng giọng persona. NGHIÊM: bạn LÀ persona — KHÔNG được đảo vai, KHÔNG xưng "em" rồi gọi đối phương là "cô/anh/chị" nếu persona là người lớn hơn. Giữ đúng đại từ persona dùng trong system prompt. Câu hỏi phải hướng VỀ đối phương, không hướng về bản thân. ${OFF_TOPIC_BAN}${moodTone}`;
        case 'random_observation':
            return `Lần này HÃY: nhận xét bâng quơ về thế giới xung quanh (thời tiết/đồ ăn/giờ giấc/đường phố). ${OFF_TOPIC_BAN}${moodTone}${topicLine(topicHint)}`;
        case 'silent':
            return null;
        default:
            return null;
    }
}

export function rememberReaction(reactionType) {
    if (!state.recentReactions) state.recentReactions = [];
    state.recentReactions.push(reactionType);
    if (state.recentReactions.length > RECENT_BUF_SIZE) {
        state.recentReactions = state.recentReactions.slice(-RECENT_BUF_SIZE);
    }
}

export function resetReactionHistory() {
    state.recentReactions = [];
    state.recentTopics = [];
}
