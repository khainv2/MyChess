// MyChess Web GUI server: static files + WebSocket bridge to MyChessUCI.
//
// Each WebSocket connection owns one MyChessUCI subprocess.
// Client sends JSON commands; server translates to UCI and streams back
// `info` / `bestmove` events as JSON.

const path = require('path');
const http = require('http');
const { spawn } = require('child_process');
const express = require('express');
const { WebSocketServer } = require('ws');

const PORT = parseInt(process.env.PORT || '8080', 10);
const HOST = process.env.HOST || '0.0.0.0';
const ENGINE_PATH = process.env.MYCHESS_UCI_PATH ||
    path.resolve(__dirname, '..', '_build_uci_nodeps', 'MyChessUCI');

const app = express();
app.use(express.static(path.join(__dirname, 'public')));

const server = http.createServer(app);
const wss = new WebSocketServer({ server });

const ts = () => new Date().toISOString();
const log = (s) => process.stdout.write(`${ts()} ${s}\n`);
const logErr = (s) => process.stderr.write(`${ts()} ${s}\n`);

// ── EngineProcess ────────────────────────────────────────────────────

class EngineProcess {
    constructor(binPath, onLine, onExit, tag) {
        this.tag = tag || '';
        this.startedAt = Date.now();
        this.killedByUs = false;
        this.proc = spawn(binPath, [], { stdio: ['pipe', 'pipe', 'pipe'] });
        this.pid = this.proc.pid;
        this.onLine = onLine;
        this.onExit = onExit;
        this.buf = '';
        this.proc.stdout.on('data', (chunk) => this._onData(chunk));
        this.proc.stderr.on('data', (chunk) => {
            // engine should not normally write to stderr; surface for debug
            logErr(`[engine ${this.tag} pid=${this.pid} stderr] ${chunk.toString().trimEnd()}`);
        });
        this.proc.on('exit', (code, signal) => {
            const lifeMs = Date.now() - this.startedAt;
            log(`[engine ${this.tag} pid=${this.pid}] exit code=${code} signal=${signal} life=${lifeMs}ms killedByUs=${this.killedByUs}`);
            onExit(code, signal);
        });
        this.proc.on('error', (err) => {
            logErr(`[engine ${this.tag} spawn error] ${err.message}`);
            onExit(-1, null);
        });
    }

    _onData(chunk) {
        this.buf += chunk.toString('utf8');
        let idx;
        while ((idx = this.buf.indexOf('\n')) >= 0) {
            const line = this.buf.slice(0, idx).replace(/\r$/, '');
            this.buf = this.buf.slice(idx + 1);
            if (line.length) this.onLine(line);
        }
    }

    send(line) {
        if (this.proc.stdin.writable) {
            this.proc.stdin.write(line + '\n');
        }
    }

    kill() {
        this.killedByUs = true;
        try { this.send('quit'); } catch (_) { /* ignore */ }
        // Hard-kill after a short grace period in case the engine ignores quit.
        setTimeout(() => {
            if (!this.proc.killed) {
                try { this.proc.kill('SIGKILL'); } catch (_) { /* ignore */ }
            }
        }, 1000).unref();
    }
}

// ── UCI line parsing ─────────────────────────────────────────────────

function parseInfoLine(line) {
    // info depth N score (cp X | mate Y) time T nodes N nps R [pv ...]
    const tokens = line.split(/\s+/);
    if (tokens[0] !== 'info') return null;
    const out = { type: 'info' };
    for (let i = 1; i < tokens.length; i++) {
        const t = tokens[i];
        if (t === 'depth')      out.depth = parseInt(tokens[++i], 10);
        else if (t === 'time')  out.time  = parseInt(tokens[++i], 10);
        else if (t === 'nodes') out.nodes = parseInt(tokens[++i], 10);
        else if (t === 'nps')   out.nps   = parseInt(tokens[++i], 10);
        else if (t === 'score') {
            const kind = tokens[++i];
            const val  = parseInt(tokens[++i], 10);
            out.score = { kind, value: val };
        }
        else if (t === 'pv') {
            out.pv = tokens.slice(i + 1);
            break;
        }
    }
    return out;
}

function parseBestmove(line) {
    // bestmove e2e4   |   bestmove (none)
    const m = line.match(/^bestmove\s+(\S+)/);
    if (!m) return null;
    const uci = m[1] === '(none)' ? null : m[1];
    return { type: 'bestmove', uci };
}

// ── WebSocket session ────────────────────────────────────────────────

let connSeq = 0;

wss.on('connection', (ws, req) => {
    const id = ++connSeq;
    const remote = req.socket.remoteAddress;
    const ua = (req.headers['user-agent'] || '').slice(0, 60);
    const connectedAt = Date.now();
    log(`[ws ${id}] connect ${remote} ua="${ua}"`);

    let initialized = false;
    let alive = true;
    let lastGoAt = 0;
    let lastBestmoveAt = 0;

    const sendJson = (obj) => {
        if (ws.readyState === ws.OPEN) ws.send(JSON.stringify(obj));
    };

    const engine = new EngineProcess(
        ENGINE_PATH,
        (line) => {
            if (line === 'uciok') {
                initialized = true;
                sendJson({ type: 'ready' });
                return;
            }
            if (line === 'readyok') return;
            if (line.startsWith('info')) {
                const info = parseInfoLine(line);
                if (info) sendJson(info);
                return;
            }
            if (line.startsWith('bestmove')) {
                lastBestmoveAt = Date.now();
                const bm = parseBestmove(line);
                if (bm) sendJson(bm);
                return;
            }
            // id name / id author / unknown — ignore
        },
        (code, signal) => {
            const wasAlive = alive;
            alive = false;
            // If we initiated the kill (ws closed first), this is an expected exit.
            if (engine && engine.killedByUs) return;
            const sinceGo = lastGoAt ? `${Date.now() - lastGoAt}ms` : 'n/a';
            const sinceBest = lastBestmoveAt ? `${Date.now() - lastBestmoveAt}ms` : 'n/a';
            logErr(`[ws ${id}] engine died UNEXPECTEDLY code=${code} signal=${signal} sinceGo=${sinceGo} sinceBestmove=${sinceBest} wsState=${ws.readyState}`);
            if (wasAlive) sendJson({ type: 'error', msg: `engine exited (code=${code} signal=${signal})` });
            try { ws.close(); } catch (_) { /* ignore */ }
        },
        `ws${id}`,
    );

    engine.send('uci');
    engine.send('isready');

    ws.on('message', (data) => {
        if (!alive) return;
        let msg;
        try { msg = JSON.parse(data.toString()); }
        catch (_) { sendJson({ type: 'error', msg: 'invalid JSON' }); return; }

        switch (msg.type) {
            case 'newgame':
                engine.send('ucinewgame');
                if (msg.fen && typeof msg.fen === 'string') {
                    engine.send(`position fen ${msg.fen}`);
                } else {
                    engine.send('position startpos');
                }
                break;

            case 'position': {
                // {fen?, moves?: [uci...]}
                let cmd;
                if (msg.fen) cmd = `position fen ${msg.fen}`;
                else         cmd = 'position startpos';
                if (Array.isArray(msg.moves) && msg.moves.length) {
                    cmd += ' moves ' + msg.moves.join(' ');
                }
                engine.send(cmd);
                break;
            }

            case 'go': {
                // {movetimeMs?, depth?}
                let cmd = 'go';
                if (Number.isInteger(msg.movetimeMs) && msg.movetimeMs > 0) {
                    cmd += ` movetime ${msg.movetimeMs}`;
                } else if (Number.isInteger(msg.depth) && msg.depth > 0) {
                    cmd += ` depth ${msg.depth}`;
                } else {
                    cmd += ' movetime 1000';
                }
                lastGoAt = Date.now();
                engine.send(cmd);
                break;
            }

            case 'stop':
                engine.send('stop');
                break;

            default:
                sendJson({ type: 'error', msg: `unknown type: ${msg.type}` });
        }
    });

    ws.on('close', (code, reason) => {
        const lifeMs = Date.now() - connectedAt;
        const reasonStr = reason && reason.length ? reason.toString() : '';
        log(`[ws ${id}] disconnect code=${code} reason="${reasonStr}" life=${lifeMs}ms alive=${alive}`);
        engine.kill();
    });

    ws.on('error', (err) => {
        logErr(`[ws ${id} error] ${err.message}`);
        engine.kill();
    });
});

server.listen(PORT, HOST, () => {
    log(`MyChess web GUI listening on http://${HOST}:${PORT}`);
    log(`Engine binary: ${ENGINE_PATH}`);
});
