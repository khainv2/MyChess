// Smoke test the new chat_request path. Requires:
//   - vLLM forward at localhost:3073 (run-bg.sh started with LLM_BASE_URL=http://localhost:3073)
//   - server running on localhost:8080
const WebSocket = require('ws');

const ws = new WebSocket('ws://localhost:8080');
let req = 0;

ws.on('open', () => console.log('[ws] open'));

ws.on('message', (raw) => {
    const msg = JSON.parse(raw.toString());
    if (msg.type === 'ready') {
        console.log('[ready] sending chat_request');
        const t0 = Date.now();
        ws.send(JSON.stringify({
            type: 'chat_request',
            requestId: 'r' + (++req),
            systemPrompt: 'Bạn là Tí Hâm, nam, 22, miền Nam, hài bựa, trẻ trâu. Trả lời 1 câu thật ngắn tiếng Việt, không emoji.',
            userPrompt: 'Đối thủ vừa đi 1.e4. Bạn nói gì khi vừa thấy nước đó?',
            maxTokens: 80,
        }));
        ws.tStart = t0;
    }
    if (msg.type === 'chat') {
        const dt = Date.now() - ws.tStart;
        console.log(`[chat] ok=${msg.ok} dt=${dt}ms`);
        if (msg.ok) console.log('  text:', JSON.stringify(msg.text));
        else console.log('  error:', msg.error);
        ws.close();
    }
});

ws.on('close', () => { console.log('[ws] closed'); process.exit(0); });
ws.on('error', (e) => { console.error('[ws] error', e.message); process.exit(1); });

setTimeout(() => { console.error('timeout'); process.exit(1); }, 15000);
