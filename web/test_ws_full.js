// Extended WebSocket integration test.
// Scenarios:
//   1. position with moves array → bestmove
//   2. stop during thinking → bestmove still arrives
//   3. EvE chain: 4 plies of engine-vs-engine using short movetime
const WebSocket = require('ws');

function open() {
    return new Promise((resolve, reject) => {
        const ws = new WebSocket('ws://127.0.0.1:8080');
        ws.once('open', () => {
            ws.once('message', (data) => {
                const msg = JSON.parse(data.toString());
                if (msg.type === 'ready') resolve(ws);
                else reject(new Error('expected ready, got ' + msg.type));
            });
        });
        ws.once('error', reject);
    });
}

function nextBestmove(ws) {
    return new Promise((resolve, reject) => {
        const onMsg = (data) => {
            const m = JSON.parse(data.toString());
            if (m.type === 'bestmove') {
                ws.off('message', onMsg);
                resolve(m.uci);
            } else if (m.type === 'error') {
                ws.off('message', onMsg);
                reject(new Error(m.msg));
            }
        };
        ws.on('message', onMsg);
    });
}

async function run() {
    let pass = 0, fail = 0;
    const check = (name, ok, extra='') => {
        console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${extra ? '  — ' + extra : ''}`);
        ok ? pass++ : fail++;
    };

    // Test 1: position with moves
    {
        const ws = await open();
        ws.send(JSON.stringify({ type: 'position', moves: ['e2e4','e7e5','g1f3'] }));
        ws.send(JSON.stringify({ type: 'go', movetimeMs: 300 }));
        const bm = await nextBestmove(ws);
        check('position+moves → bestmove', !!bm && bm.length >= 4, `got ${bm}`);
        ws.close();
    }

    // Test 2: stop during thinking
    {
        const ws = await open();
        ws.send(JSON.stringify({ type: 'newgame' }));
        ws.send(JSON.stringify({ type: 'go', movetimeMs: 5000 }));
        // Send stop after 100ms
        await new Promise(r => setTimeout(r, 100));
        const t0 = Date.now();
        ws.send(JSON.stringify({ type: 'stop' }));
        const bm = await nextBestmove(ws);
        const elapsed = Date.now() - t0;
        check('stop returns bestmove quickly', !!bm && elapsed < 1500, `got ${bm} after ${elapsed}ms post-stop`);
        ws.close();
    }

    // Test 3: EvE chain (simulate frontend driving both sides)
    {
        const ws = await open();
        ws.send(JSON.stringify({ type: 'newgame' }));
        const moves = [];
        for (let i = 0; i < 4; i++) {
            ws.send(JSON.stringify({ type: 'position', moves: [...moves] }));
            ws.send(JSON.stringify({ type: 'go', movetimeMs: 200 }));
            const bm = await nextBestmove(ws);
            if (!bm) { check('EvE chain step ' + i, false, 'null bestmove'); ws.close(); return; }
            moves.push(bm);
        }
        check('EvE chain (4 plies)', moves.length === 4, moves.join(' '));
        ws.close();
    }

    console.log(`\n${pass}/${pass+fail} passed`);
    process.exit(fail ? 1 : 0);
}

run().catch(err => { console.error(err); process.exit(1); });
