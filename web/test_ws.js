// One-off WebSocket smoke test for the MyChess web bridge.
const WebSocket = require('ws');
const ws = new WebSocket('ws://127.0.0.1:8080');

let infoCount = 0;
ws.on('open', () => {
    console.log('[ws] open');
});
ws.on('message', (data) => {
    const msg = JSON.parse(data.toString());
    if (msg.type === 'ready') {
        console.log('[ready] sending newgame + go');
        ws.send(JSON.stringify({ type: 'newgame' }));
        ws.send(JSON.stringify({ type: 'go', movetimeMs: 800 }));
    } else if (msg.type === 'info') {
        infoCount++;
        if (msg.depth && msg.depth <= 3) {
            console.log(`[info] depth=${msg.depth} score=${JSON.stringify(msg.score)} pv=${(msg.pv || []).slice(0, 3).join(' ')}`);
        }
    } else if (msg.type === 'bestmove') {
        console.log(`[bestmove] ${msg.uci}  (received ${infoCount} info lines)`);
        ws.close();
    } else if (msg.type === 'error') {
        console.error('[error]', msg.msg);
        process.exit(1);
    }
});
ws.on('close', () => { console.log('[ws] closed'); process.exit(0); });
ws.on('error', (err) => { console.error('[ws error]', err.message); process.exit(1); });

setTimeout(() => { console.error('timeout'); process.exit(2); }, 5000);
