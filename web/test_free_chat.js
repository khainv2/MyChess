// Verify free-chat fix: when chatHistory has auto-commentary turns (with EVENT_TAG)
// followed by a plain user "xin chào", the bot should greet naturally — NOT
// drag chess moves into a casual greeting.
const WebSocket = require('ws');

const EVENT_TAG = '[SỰ-KIỆN-BÀN-CỜ]';

const PERSONA_SYS = `Bạn là Bé Mochi, nữ, 19 tuổi, sinh viên miền Bắc, đánh cờ vua trình ~1300 elo. Tính cách: hài tăng động, nói nhiều, dễ tính, hay nhõng nhẽo. Văn phong: xưng "tớ/cậu" hoặc "em/anh" tùy đối phương, hay dùng "trời ơi", "ơ kìa", "ehe", "oách xà lách", có lúc nhõng nhẽo "đừng mà", "thôi mà". Trả lời 1-2 câu, không emoji, giọng tươi vui kiểu streamer.`;

function buildFreeChatSystem() {
    return [
        PERSONA_SYS,
        '',
        '— Bối cảnh trận đấu (CHỈ tham khảo nội bộ; KHÔNG đem ra nói nếu người chơi không hỏi về cờ) —',
        'Bạn cầm bên Đen, người chơi cầm bên Trắng.',
        'Lượt hiện tại: Đen đi.',
        'Vật chất: thế cờ cân bằng về quân.',
        'Lịch sử nước cờ (SAN): e4 e5 Nc3.',
        'FEN: rnbqkbnr/pppp1ppp/8/4p3/4P3/2N5/PPPP1PPP/R1BQKBNR b KQkq - 1 2.',
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
    ].join('\n');
}

// Simulated chatHistory after a few moves (auto-commentary turns).
const fakeHistory = [
    { role: 'user', content: `${EVENT_TAG} Đối thủ vừa đi quân Tốt (ký hiệu e4). Tình thế tổng quan: thế cờ cân bằng. Lịch sử gần đây: e4. Hãy phản ứng 1 câu ngắn theo đúng tính cách.` },
    { role: 'assistant', content: 'Trời ơi cậu mở e4 luôn à, tớ thích người gan dạ đó.' },
    { role: 'user', content: `${EVENT_TAG} Bạn vừa đi quân Tốt (ký hiệu e5). Tình thế tổng quan: thế cờ cân bằng. Lịch sử gần đây: e4 e5. Hãy nói 1 câu thật ngắn.` },
    { role: 'assistant', content: 'Tớ chơi e5 đáp lễ, công bằng nhỉ.' },
    { role: 'user', content: `${EVENT_TAG} Đối thủ vừa đi quân Mã (ký hiệu Nc3). Tình thế tổng quan: thế cờ cân bằng. Lịch sử gần đây: e4 e5 Nc3. Hãy phản ứng 1 câu ngắn.` },
    { role: 'assistant', content: 'Ehe Vienna đó hả, hơi hung đó nha.' },
];

const ws = new WebSocket('ws://localhost:8080');
let req = 0;
function ask(userText) {
    return new Promise((resolve, reject) => {
        const requestId = 'r' + (++req);
        const messages = [
            { role: 'system', content: buildFreeChatSystem() },
            ...fakeHistory,
            { role: 'user', content: userText },
        ];
        const t0 = Date.now();
        const handler = (raw) => {
            const m = JSON.parse(raw.toString());
            if (m.type !== 'chat' || m.requestId !== requestId) return;
            ws.off('message', handler);
            if (!m.ok) return reject(new Error(m.error));
            resolve({ text: m.text, dt: Date.now() - t0 });
        };
        ws.on('message', handler);
        ws.send(JSON.stringify({ type: 'chat_request', requestId, messages, maxTokens: 220 }));
    });
}

ws.on('message', async (raw) => {
    const m = JSON.parse(raw.toString());
    if (m.type !== 'ready') return;
    try {
        // Test 1: small talk — should NOT mention chess moves
        let r = await ask('xin chào');
        console.log(`[Test 1: "xin chào"]\n  ${r.dt}ms: "${r.text}"`);
        const mentionsMoves1 = /e[2-7]|N[cf][1-8]|nước|cờ|đi/i.test(r.text);
        console.log(`  → mentions chess: ${mentionsMoves1 ? 'YES (bad)' : 'NO (good)'}\n`);

        // Test 2: hỏi thăm — should NOT mention moves
        r = await ask('cậu khoẻ không?');
        console.log(`[Test 2: "cậu khoẻ không?"]\n  ${r.dt}ms: "${r.text}"`);
        const mentionsMoves2 = /e[2-7]|N[cf][1-8]|nước|cờ|đi/i.test(r.text);
        console.log(`  → mentions chess: ${mentionsMoves2 ? 'YES (bad)' : 'NO (good)'}\n`);

        // Test 3: hỏi rõ về cờ — SHOULD respond about the position
        r = await ask('thế cờ giờ thế nào?');
        console.log(`[Test 3: "thế cờ giờ thế nào?"]\n  ${r.dt}ms: "${r.text}"`);
        const mentionsMoves3 = /Vienna|cân|đỡ|nước|cờ|tình|thế|đẹp|ổn|được/i.test(r.text);
        console.log(`  → mentions chess: ${mentionsMoves3 ? 'YES (good)' : 'NO (bad)'}\n`);

    } catch (e) {
        console.error('[err]', e.message);
    } finally {
        ws.close();
    }
});
ws.on('close', () => process.exit(0));
ws.on('error', e => { console.error('ws err', e.message); process.exit(1); });
setTimeout(() => { console.error('timeout'); process.exit(1); }, 60000);
