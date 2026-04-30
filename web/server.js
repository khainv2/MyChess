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

const LLM_BASE_URL = process.env.LLM_BASE_URL || 'http://192.168.205.172:3073';
const LLM_MODEL    = process.env.LLM_MODEL    || 'qwen3.6-35b-a3b';
const LLM_TIMEOUT_MS = parseInt(process.env.LLM_TIMEOUT_MS || '8000', 10);

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

// ── LLM (vLLM Qwen3.6 OpenAI-compat) ─────────────────────────────────

async function callLLM(messages, maxTokens = 90, opts = {}) {
    if (!Array.isArray(messages) || messages.length === 0) {
        throw new Error('callLLM: messages must be a non-empty array');
    }
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), LLM_TIMEOUT_MS);
    const body = {
        model: LLM_MODEL,
        messages,
        max_tokens: maxTokens,
        temperature: typeof opts.temperature === 'number' ? opts.temperature : 0.85,
        top_p: typeof opts.top_p === 'number' ? opts.top_p : 0.9,
        chat_template_kwargs: { enable_thinking: false },
    };
    if (typeof opts.seed === 'number' && Number.isFinite(opts.seed)) {
        body.seed = Math.floor(opts.seed);
    }
    try {
        const res = await fetch(`${LLM_BASE_URL}/v1/chat/completions`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            signal: ctrl.signal,
            body: JSON.stringify(body),
        });
        if (!res.ok) throw new Error(`LLM HTTP ${res.status}`);
        const data = await res.json();
        const msg = data?.choices?.[0]?.message || {};
        const text = (msg.content || '').trim();
        if (!text) throw new Error('empty content');
        return text;
    } finally {
        clearTimeout(timer);
    }
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

    // ── Precompute engine (Phase 3) ────────────────────────────────────
    // Engine thứ 2 chạy song song, chuyên dùng để tính top-1 nước cho phía
    // user trước khi user đi. Dùng làm nguồn cho "tiên tri" và so sánh
    // hậu kiểm. Tách ra để không race với engine chính.
    let preReqId = null;          // requestId hiện tại
    let preLastInfo = null;       // info cuối cùng (lưu để có score)
    const preEngine = new EngineProcess(
        ENGINE_PATH,
        (line) => {
            if (line === 'uciok' || line === 'readyok') return;
            if (line.startsWith('info')) {
                const info = parseInfoLine(line);
                if (info && info.score) preLastInfo = info;
                return;
            }
            if (line.startsWith('bestmove')) {
                const bm = parseBestmove(line);
                const reqId = preReqId;
                if (!reqId) return;          // không ai chờ
                const score = preLastInfo?.score || null;
                preReqId = null;
                preLastInfo = null;
                sendJson({
                    type: 'precompute',
                    requestId: reqId,
                    ok: true,
                    uci: bm?.uci || null,
                    score,
                });
            }
        },
        () => { /* die quietly; main engine death handles teardown */ },
        `ws${id}-pre`,
    );
    preEngine.send('uci');
    preEngine.send('isready');

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

            case 'precompute_request': {
                // {requestId, fen, moves?, movetimeMs?}
                // Nếu đang có request cũ chưa xong → stop và override.
                if (preReqId) preEngine.send('stop');
                preReqId = msg.requestId || `pre-${Date.now()}`;
                preLastInfo = null;
                let posCmd;
                if (typeof msg.fen === 'string' && msg.fen) posCmd = `position fen ${msg.fen}`;
                else                                          posCmd = 'position startpos';
                if (Array.isArray(msg.moves) && msg.moves.length) {
                    posCmd += ' moves ' + msg.moves.join(' ');
                }
                preEngine.send(posCmd);
                const mt = (Number.isInteger(msg.movetimeMs) && msg.movetimeMs > 0)
                    ? msg.movetimeMs : 400;
                preEngine.send(`go movetime ${mt}`);
                break;
            }

            case 'chat_request': {
                // {requestId, messages: [{role, content}, ...], maxTokens?}
                // Legacy form {systemPrompt, userPrompt} also accepted.
                const reqId = msg.requestId || null;
                let messages;
                if (Array.isArray(msg.messages) && msg.messages.length) {
                    // Sanitize: drop bad entries, cap each content + total count.
                    messages = msg.messages
                        .filter(m => m && typeof m.role === 'string' && typeof m.content === 'string')
                        .slice(-200)
                        .map(m => ({ role: m.role, content: m.content.slice(0, 4000) }));
                } else if (msg.systemPrompt && msg.userPrompt) {
                    messages = [
                        { role: 'system', content: String(msg.systemPrompt).slice(0, 4000) },
                        { role: 'user',   content: String(msg.userPrompt).slice(0, 4000) },
                    ];
                }
                if (!messages || !messages.length) {
                    sendJson({ type: 'chat', requestId: reqId, ok: false, error: 'missing messages' });
                    break;
                }
                const llmOpts = {};
                if (typeof msg.temperature === 'number') llmOpts.temperature = msg.temperature;
                if (typeof msg.top_p === 'number')       llmOpts.top_p       = msg.top_p;
                if (typeof msg.seed === 'number')        llmOpts.seed        = msg.seed;
                callLLM(messages, msg.maxTokens || 90, llmOpts)
                    .then(text => sendJson({ type: 'chat', requestId: reqId, ok: true, text }))
                    .catch(err => {
                        logErr(`[ws ${id} chat] ${err.message}`);
                        sendJson({ type: 'chat', requestId: reqId, ok: false, error: err.message });
                    });
                break;
            }

            default:
                sendJson({ type: 'error', msg: `unknown type: ${msg.type}` });
        }
    });

    ws.on('close', (code, reason) => {
        const lifeMs = Date.now() - connectedAt;
        const reasonStr = reason && reason.length ? reason.toString() : '';
        log(`[ws ${id}] disconnect code=${code} reason="${reasonStr}" life=${lifeMs}ms alive=${alive}`);
        engine.kill();
        preEngine.kill();
    });

    ws.on('error', (err) => {
        logErr(`[ws ${id} error] ${err.message}`);
        engine.kill();
        preEngine.kill();
    });
});

server.listen(PORT, HOST, () => {
    log(`MyChess web GUI listening on http://${HOST}:${PORT}`);
    log(`Engine binary: ${ENGINE_PATH}`);
});
