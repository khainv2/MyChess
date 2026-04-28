// Verify the new messages[] path: send 3 turns where each later turn references
// what the assistant said before, proving real conversation memory.
const WebSocket = require('ws');

const ws = new WebSocket('ws://localhost:8080');
let req = 0;
const SYS = "Bạn là Tí Hâm, nam, 22 tuổi, miền Nam, hài bựa, trẻ trâu. Trả lời 1 câu thật ngắn, không emoji.";
const history = [];

function ask(userText) {
    return new Promise((resolve, reject) => {
        const requestId = 'r' + (++req);
        const messages = [
            { role: 'system', content: SYS },
            ...history,
            { role: 'user', content: userText },
        ];
        const t0 = Date.now();
        const handler = (raw) => {
            const m = JSON.parse(raw.toString());
            if (m.type !== 'chat' || m.requestId !== requestId) return;
            ws.off('message', handler);
            if (!m.ok) return reject(new Error(m.error));
            history.push({ role: 'user', content: userText });
            history.push({ role: 'assistant', content: m.text });
            resolve({ text: m.text, dt: Date.now() - t0 });
        };
        ws.on('message', handler);
        ws.send(JSON.stringify({ type: 'chat_request', requestId, messages, maxTokens: 90 }));
    });
}

ws.on('open', () => console.log('[ws] open'));
ws.on('message', async (raw) => {
    const m = JSON.parse(raw.toString());
    if (m.type !== 'ready') return;
    try {
        let r = await ask('Đối thủ vừa đi 1.e4. Bạn nói gì?');
        console.log(`[turn 1] ${r.dt}ms: "${r.text}"`);

        r = await ask('Đối thủ vừa đi 2.Nf3. Bạn nói gì? (Lưu ý: bạn vẫn có thể nhắc đến những gì mình đã nói lúc nãy.)');
        console.log(`[turn 2] ${r.dt}ms: "${r.text}"`);

        r = await ask('Đối thủ vừa đi 3.Bb5 (Ruy Lopez). Bạn vừa nói 2 câu rồi, giờ nói câu thứ 3 — đừng lặp lại ý đã nói, có thể callback nhẹ tới câu trước.');
        console.log(`[turn 3] ${r.dt}ms: "${r.text}"`);

        console.log(`\n[history size] ${history.length} messages = ${history.length/2} turn pairs`);
    } catch (e) {
        console.error('[err]', e.message);
    } finally {
        ws.close();
    }
});
ws.on('close', () => process.exit(0));
ws.on('error', e => { console.error('ws err', e.message); process.exit(1); });
setTimeout(() => { console.error('timeout'); process.exit(1); }, 30000);
