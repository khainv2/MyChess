// MyChess web GUI controller.
// Uses chess.js for rule logic on the client; talks to MyChessUCI via the
// Node WebSocket bridge.

import { Chess } from './vendor/chess.js';
import { Board3D } from './board3d.js';

// ── DOM ──────────────────────────────────────────────────────────────

const $ = (id) => document.getElementById(id);
const boardEl = $('board');
const statusEl = $('status');
const filesTop = $('files-top'), filesBottom = $('files-bottom');
const ranksLeft = $('ranks-left'), ranksRight = $('ranks-right');
const fenEl = $('fen');
const histEl = $('history');
const modeEl = $('mode');
const humanSideEl = $('human-side');
const levelEl = $('level');
const timeControlEl = $('time-control');
const thinktimeEl = $('thinktime');
const thinktimeLabel = $('thinktime-label');
const rowThinktime = $('row-thinktime');
const rowSide = $('row-side');
const rowLevel = $('row-level');
const promoOverlay = $('promo-overlay');
const infoDepth = $('info-depth'), infoScore = $('info-score');
const infoNodes = $('info-nodes'), infoNps = $('info-nps');
const infoPv = $('info-pv');
const playerTopName = $('player-top-name');
const playerBottomName = $('player-bottom-name');
const playerTopIcon = $('player-top-icon');
const playerBottomIcon = $('player-bottom-icon');
const clockTopEl = $('clock-top');
const clockBottomEl = $('clock-bottom');
const endDialog = $('end-dialog');
const endTitleEl = $('end-title');
const endMsgEl = $('end-msg');
const endIconEl = $('end-icon');
const btnSound = $('btn-sound');
const btnHint = $('btn-hint');
const btnResign = $('btn-resign');
const btnUndo = $('btn-undo');

const FILES = ['a','b','c','d','e','f','g','h'];

// ── State ────────────────────────────────────────────────────────────

const game = new Chess();
let flipped = false;
let selected = null;          // square algebra ("e2") or null
let legalForSelected = [];    // chess.js verbose moves from selected
let lastMove = null;          // {from, to}
let historySAN = [];          // for display
let viewIdx = -1;             // which history index is being viewed (-1 = live)
let engineReady = false;
let waitingBestmove = false;
let pendingHint = false;
let mode = 'pve';             // 'pve' | 'eve' | 'pvp'
let gameOver = false;
let board3d = null;
let view3D = localStorage.getItem('mychess.view3d') === 'on';

// ── WebSocket ────────────────────────────────────────────────────────

const wsUrl = (location.protocol === 'https:' ? 'wss:' : 'ws:') + '//' + location.host;
let ws = null;
let reconnectAttempts = 0;
let reconnectTimer = null;
let resumingSession = false;
const MAX_RECONNECT = 5;

function connect() {
    if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
    const reconnectBtn = $('btn-reconnect');
    ws = new WebSocket(wsUrl);
    ws.addEventListener('open', () => {
        reconnectAttempts = 0;
        if (reconnectBtn) reconnectBtn.style.display = 'none';
        setStatus(resumingSession ? 'Đang khôi phục phiên…' : 'Đang khởi tạo máy…');
    });
    ws.addEventListener('close', () => {
        engineReady = false;
        waitingBestmove = false;
        if (reconnectAttempts < MAX_RECONNECT) {
            reconnectAttempts++;
            const delay = Math.min(8000, 1000 * Math.pow(2, reconnectAttempts - 1));
            setStatus(`Mất kết nối. Thử lại ${reconnectAttempts}/${MAX_RECONNECT} sau ${Math.round(delay/1000)}s…`);
            resumingSession = true;
            reconnectTimer = setTimeout(connect, delay);
        } else {
            setStatus('Mất kết nối máy. Bấm "Kết nối lại" để thử lại.');
            if (reconnectBtn) reconnectBtn.style.display = '';
        }
    });
    ws.addEventListener('error', () => { /* close fires too — handled there */ });
    ws.addEventListener('message', (ev) => {
        let msg;
        try { msg = JSON.parse(ev.data); } catch (_) { return; }
        handleServerMessage(msg);
    });
}

function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
}

function handleServerMessage(msg) {
    switch (msg.type) {
        case 'ready':
            engineReady = true;
            if (resumingSession) {
                resumingSession = false;
                syncEngine();
            } else {
                sendNewGame();
            }
            updateStatus();
            updateButtons();
            maybeTriggerEngine();
            break;
        case 'info':
            renderInfo(msg);
            break;
        case 'bestmove':
            waitingBestmove = false;
            clearInfo();
            if (pendingHint) {
                pendingHint = false;
                if (msg.uci) showHint(msg.uci);
                if (timeControl && !gameOver) startClock();
                updateStatus();
                updateButtons();
                return;
            }
            if (msg.uci) applyEngineMove(msg.uci);
            else { updateStatus(); updateButtons(); }
            break;
        case 'error':
            setStatus('Máy báo lỗi: ' + msg.msg);
            break;
    }
}

// ── Board rendering ──────────────────────────────────────────────────

function squareName(rank, file) { return FILES[file] + (rank + 1); }

function buildBoard() {
    boardEl.innerHTML = '';
    for (let displayRow = 0; displayRow < 8; displayRow++) {
        for (let displayCol = 0; displayCol < 8; displayCol++) {
            const rank = flipped ? displayRow : 7 - displayRow;
            const file = flipped ? 7 - displayCol : displayCol;
            const sq = document.createElement('div');
            sq.className = 'sq ' + (((rank + file) % 2 === 0) ? 'dark' : 'light');
            sq.dataset.sq = squareName(rank, file);
            boardEl.appendChild(sq);
        }
    }

    const fileLabels = flipped ? [...FILES].reverse() : FILES;
    const rankLabels = flipped ? [1,2,3,4,5,6,7,8] : [8,7,6,5,4,3,2,1];
    filesTop.innerHTML = filesBottom.innerHTML =
        fileLabels.map(f => `<div>${f}</div>`).join('');
    ranksLeft.innerHTML = ranksRight.innerHTML =
        rankLabels.map(r => `<div>${r}</div>`).join('');
}

function renderPosition() {
    const display = boardForView();

    document.querySelectorAll('.sq').forEach(sq => {
        sq.classList.remove('from','to','check','selected','has-piece','hint');
        sq.innerHTML = '';
        const piece = display.get(sq.dataset.sq);
        if (piece) {
            sq.classList.add('has-piece');
            const div = document.createElement('div');
            div.className = 'piece';
            const color = piece.color === 'w' ? 'w' : 'b';
            const type = piece.type.toUpperCase();
            div.style.backgroundImage = `url("pieces/${color}${type}.svg")`;
            div.draggable = canDragLive() && piece.color === sideToMoveLive() && viewIdx === -1;
            div.dataset.sq = sq.dataset.sq;
            sq.appendChild(div);
        }
        const dot = document.createElement('div');
        dot.className = 'legal-dot';
        dot.style.display = 'none';
        sq.appendChild(dot);
    });

    if (lastMove && viewIdx === -1) {
        const a = sqEl(lastMove.from), b = sqEl(lastMove.to);
        if (a) a.classList.add('from');
        if (b) b.classList.add('to');
    }

    if (display.inCheck()) {
        const turn = display.turn();
        const kingSq = findKingSquare(display, turn);
        if (kingSq) sqEl(kingSq).classList.add('check');
    }

    if (selected) {
        const s = sqEl(selected);
        if (s) s.classList.add('selected');
        for (const m of legalForSelected) {
            const d = sqEl(m.to);
            if (d) {
                const dot = d.querySelector('.legal-dot');
                if (dot) dot.style.display = 'block';
            }
        }
    }

    fenEl.value = game.fen();
    syncBoard3D();
}

function syncBoard3D() {
    if (!board3d || !view3D) return;
    const display = boardForView();
    board3d.setBoardFromFen(display.fen());
    let checkSq = null;
    if (display.inCheck()) checkSq = findKingSquare(display, display.turn());
    board3d.setHighlights({
        from: viewIdx === -1 ? lastMove?.from : null,
        to:   viewIdx === -1 ? lastMove?.to   : null,
        selected: viewIdx === -1 ? selected : null,
        legalSquares: viewIdx === -1 ? legalForSelected.map(m => m.to) : [],
        checkSquare: checkSq,
    });
}

function sqEl(name) { return boardEl.querySelector(`.sq[data-sq="${name}"]`); }

function findKingSquare(chess, color) {
    const board = chess.board();
    for (let r = 0; r < 8; r++) for (let f = 0; f < 8; f++) {
        const p = board[r][f];
        if (p && p.type === 'k' && p.color === color) {
            return FILES[f] + (8 - r);
        }
    }
    return null;
}

function boardForView() {
    if (viewIdx === -1) return game;
    const tmp = new Chess();
    const moves = game.history({ verbose: true });
    for (let i = 0; i <= viewIdx && i < moves.length; i++) {
        tmp.move({ from: moves[i].from, to: moves[i].to, promotion: moves[i].promotion });
    }
    return tmp;
}

// ── Selection / move logic ──────────────────────────────────────────

function canDragLive() {
    if (gameOver) return false;
    if (mode === 'eve') return false;
    if (waitingBestmove) return false;
    if (game.isGameOver()) return false;
    return true;
}

function sideToMoveLive() { return game.turn(); }

function isHumanTurn() {
    if (mode === 'pvp') return true;
    if (mode === 'eve') return false;
    const humanColor = humanSideEl.value === 'white' ? 'w' : 'b';
    return game.turn() === humanColor;
}

function selectSquare(sq) {
    selected = sq;
    legalForSelected = game.moves({ square: sq, verbose: true });
    renderPosition();
}
function clearSelection() {
    selected = null;
    legalForSelected = [];
    renderPosition();
}

async function tryUserMove(from, to) {
    const candidates = game.moves({ square: from, verbose: true })
        .filter(m => m.to === to);
    if (!candidates.length) { clearSelection(); return false; }

    let promo = undefined;
    if (candidates.some(m => m.promotion)) {
        promo = await askPromotion();
        if (!promo) { clearSelection(); return false; }
    }

    let result;
    try { result = game.move({ from, to, promotion: promo }); }
    catch (_) { result = null; }
    if (!result) { clearSelection(); return false; }

    onMoveDone(result);
    return true;
}

function onMoveDone(result) {
    selected = null; legalForSelected = [];
    lastMove = { from: result.from, to: result.to };
    historySAN.push(result.san);
    viewIdx = -1;

    // Clock: increment to mover, then it's the other side's turn.
    applyIncrement(result.color);

    // Sound for the move type.
    playMoveSound(result);

    renderPosition();
    renderHistory();
    syncEngine();

    if (checkAndAnnounceEnd()) {
        renderClocks();
        updateButtons();
        return;
    }

    renderClocks();
    updateStatus();
    updateButtons();
    maybeTriggerEngine();
}

function applyEngineMove(uci) {
    if (uci.length < 4) return;
    const from = uci.slice(0, 2), to = uci.slice(2, 4);
    const promo = uci.length >= 5 ? uci[4] : undefined;
    let result;
    try { result = game.move({ from, to, promotion: promo }); }
    catch (_) { result = null; }
    if (!result) {
        setStatus('Máy trả về nước không hợp lệ: ' + uci);
        return;
    }
    onMoveDone(result);
}

// ── Engine ────────────────────────────────────────────────────────────

function sendNewGame() {
    send({ type: 'newgame' });
}
function syncEngine() {
    const uciHistory = game.history({ verbose: true })
        .map(m => m.from + m.to + (m.promotion || ''));
    send({ type: 'position', moves: uciHistory });
}
function currentMovetimeMs() {
    if (levelEl.value === 'custom') {
        return parseInt(thinktimeEl.value, 10) || 1000;
    }
    return parseInt(levelEl.value, 10) || 1000;
}
function maybeTriggerEngine() {
    if (!engineReady) return;
    if (gameOver) return;
    if (game.isGameOver()) return;
    if (mode === 'pvp') return;
    if (mode === 'pve' && isHumanTurn()) return;
    waitingBestmove = true;
    if (timeControl) startClock();
    updateStatus();
    updateButtons();
    send({ type: 'go', movetimeMs: currentMovetimeMs() });
}

// ── Promotion dialog ────────────────────────────────────────────────

function askPromotion() {
    const turn = game.turn();
    const color = turn === 'w' ? 'w' : 'b';
    promoOverlay.querySelectorAll('button').forEach(btn => {
        const p = btn.dataset.p.toUpperCase();
        const img = btn.querySelector('img');
        img.src = `pieces/${color}${p}.svg`;
    });
    promoOverlay.classList.remove('hidden');
    return new Promise(resolve => {
        const onClick = (ev) => {
            const btn = ev.target.closest('button[data-p]');
            if (!btn) return;
            cleanup();
            resolve(btn.dataset.p);
        };
        const onKey = (ev) => {
            if (ev.key === 'Escape') { cleanup(); resolve(null); }
        };
        function cleanup() {
            promoOverlay.classList.add('hidden');
            promoOverlay.removeEventListener('click', onClick);
            document.removeEventListener('keydown', onKey);
        }
        promoOverlay.addEventListener('click', onClick);
        document.addEventListener('keydown', onKey);
    });
}

// ── History panel ───────────────────────────────────────────────────

function renderHistory() {
    histEl.innerHTML = '';
    for (let i = 0; i < historySAN.length; i += 2) {
        const num = (i / 2) + 1;
        const li = document.createElement('li');
        li.dataset.idx = i;
        li.innerHTML =
            `<span style="color:#888">${num}.</span>` +
            `<span class="san" data-ply="${i}">${historySAN[i] || ''}</span>` +
            (historySAN[i+1] ? `<span class="san" data-ply="${i+1}">${historySAN[i+1]}</span>` : '');
        histEl.appendChild(li);
    }
    const cur = histEl.querySelector(`[data-ply="${viewIdx === -1 ? historySAN.length - 1 : viewIdx}"]`);
    if (cur) cur.parentElement.classList.add('current');
}

histEl.addEventListener('click', (ev) => {
    const span = ev.target.closest('[data-ply]');
    if (!span) return;
    const ply = parseInt(span.dataset.ply, 10);
    viewIdx = (ply === historySAN.length - 1) ? -1 : ply;
    renderPosition();
    renderHistory();
});

// ── Status / info ───────────────────────────────────────────────────

function setStatus(text) { statusEl.textContent = text; }

function updateStatus() {
    if (!engineReady) { setStatus('Đang kết nối máy…'); return; }
    if (gameOver) return;
    if (game.isCheckmate()) {
        const winner = game.turn() === 'w' ? 'Đen' : 'Trắng';
        setStatus(`Chiếu hết. ${winner} thắng.`);
        return;
    }
    if (game.isStalemate()) { setStatus('Hết nước (stalemate). Hòa.'); return; }
    if (game.isInsufficientMaterial()) { setStatus('Hòa (không đủ quân).'); return; }
    if (game.isThreefoldRepetition()) { setStatus('Hòa (lặp ba lần).'); return; }
    if (game.isDraw()) { setStatus('Hòa (luật 50 nước).'); return; }
    const turn = game.turn() === 'w' ? 'Trắng' : 'Đen';
    if (waitingBestmove && pendingHint) setStatus(`Đang tính gợi ý…`);
    else if (waitingBestmove) setStatus(`Máy đang suy nghĩ (${turn})…`);
    else if (game.inCheck()) setStatus(`${turn} bị chiếu — đến lượt ${turn}.`);
    else setStatus(`Lượt: ${turn}`);
}

function clearInfo() {
    infoDepth.textContent = '–';
    infoScore.textContent = '–';
    infoNodes.textContent = '–';
    infoNps.textContent = '–';
    infoPv.textContent = '–';
}

function renderInfo(msg) {
    if (msg.depth != null) infoDepth.textContent = msg.depth;
    if (msg.nodes != null) infoNodes.textContent = msg.nodes.toLocaleString();
    if (msg.nps != null) infoNps.textContent = msg.nps.toLocaleString();
    if (msg.score) {
        let perspective = msg.score.value;
        if (game.turn() === 'b') perspective = -perspective;
        if (msg.score.kind === 'cp') {
            const sign = perspective >= 0 ? '+' : '';
            infoScore.textContent = `${sign}${(perspective / 100).toFixed(2)}`;
        } else if (msg.score.kind === 'mate') {
            infoScore.textContent = `M${msg.score.value}`;
        }
    }
    if (msg.pv) {
        try {
            const tmp = new Chess(game.fen());
            const sans = [];
            for (const u of msg.pv) {
                if (u.length < 4) break;
                const mv = tmp.move({
                    from: u.slice(0,2), to: u.slice(2,4),
                    promotion: u.length >= 5 ? u[4] : undefined,
                });
                if (!mv) break;
                sans.push(mv.san);
            }
            infoPv.textContent = sans.join(' ');
        } catch (_) {
            infoPv.textContent = msg.pv.join(' ');
        }
    }
}

// ── Player names ────────────────────────────────────────────────────

function topPlayerColor() {
    // Without flip: white is at bottom, so black is on top.
    // With flip: white at top.
    return flipped ? 'w' : 'b';
}

function levelLabel() {
    const opt = levelEl.options[levelEl.selectedIndex];
    if (!opt) return 'Vừa';
    // Strip the "(1s)" suffix if present.
    return opt.textContent.replace(/\s*\([^)]*\)\s*$/, '');
}

function renderPlayers() {
    let names, icons;
    if (mode === 'pve') {
        const human = humanSideEl.value === 'white' ? 'w' : 'b';
        const engine = human === 'w' ? 'b' : 'w';
        names = {};
        names[human] = 'Bạn';
        names[engine] = `Máy (${levelLabel()})`;
        icons = {};
        icons[human] = '👤';
        icons[engine] = '🤖';
    } else if (mode === 'eve') {
        names = { w: `Máy Trắng (${levelLabel()})`, b: `Máy Đen (${levelLabel()})` };
        icons = { w: '🤖', b: '🤖' };
    } else { // pvp
        names = { w: 'Trắng', b: 'Đen' };
        icons = { w: '👤', b: '👤' };
    }
    const top = topPlayerColor();
    const bot = top === 'w' ? 'b' : 'w';
    playerTopName.textContent = names[top];
    playerBottomName.textContent = names[bot];
    playerTopIcon.textContent = icons[top];
    playerBottomIcon.textContent = icons[bot];
}

// ── Clock system ────────────────────────────────────────────────────

let timeControl = null; // { initial: ms, increment: ms } | null
let clockMs = { w: 0, b: 0 };
let clockRunning = false;
let clockTickHandle = null;
let lastTickTs = 0;

function setupClockFromSelect() {
    const tcStr = timeControlEl.value;
    if (tcStr === 'none') {
        timeControl = null;
        clockMs = { w: 0, b: 0 };
        stopClock();
        renderClocks();
        return;
    }
    const [secs, inc] = tcStr.split('+').map(Number);
    timeControl = { initial: secs * 1000, increment: inc * 1000 };
    clockMs = { w: timeControl.initial, b: timeControl.initial };
    stopClock();
    renderClocks();
}

function formatMs(ms) {
    if (ms < 0) ms = 0;
    if (ms < 10000) {
        const totalSec = Math.floor(ms / 1000);
        const tenths = Math.floor((ms % 1000) / 100);
        return `${totalSec}.${tenths}`;
    }
    const totalSec = Math.ceil(ms / 1000);
    const m = Math.floor(totalSec / 60);
    const s = totalSec % 60;
    return `${m}:${String(s).padStart(2,'0')}`;
}

function renderClocks() {
    if (!timeControl) {
        clockTopEl.textContent = '';
        clockBottomEl.textContent = '';
        clockTopEl.className = 'clock';
        clockBottomEl.className = 'clock';
        return;
    }
    const top = topPlayerColor();
    const bot = top === 'w' ? 'b' : 'w';
    clockTopEl.textContent = formatMs(clockMs[top]);
    clockBottomEl.textContent = formatMs(clockMs[bot]);
    const stm = game.turn();
    setClockClass(clockTopEl, top, stm);
    setClockClass(clockBottomEl, bot, stm);
}

function setClockClass(el, color, stm) {
    el.classList.remove('active', 'warn', 'dead');
    if (clockMs[color] <= 0) { el.classList.add('dead'); return; }
    if (clockRunning && stm === color && !gameOver) el.classList.add('active');
    if (clockMs[color] < 30000) el.classList.add('warn');
}

function startClock() {
    if (!timeControl || gameOver) return;
    if (game.isGameOver()) return;
    if (clockRunning) return;
    clockRunning = true;
    lastTickTs = performance.now();
    if (clockTickHandle) clearInterval(clockTickHandle);
    clockTickHandle = setInterval(tickClock, 100);
    renderClocks();
}
function stopClock() {
    clockRunning = false;
    if (clockTickHandle) { clearInterval(clockTickHandle); clockTickHandle = null; }
    renderClocks();
}
function tickClock() {
    if (!clockRunning || !timeControl) return;
    const now = performance.now();
    const delta = now - lastTickTs;
    lastTickTs = now;
    const stm = game.turn();
    clockMs[stm] -= delta;
    if (clockMs[stm] <= 0) {
        clockMs[stm] = 0;
        stopClock();
        onTimeOut(stm);
        return;
    }
    renderClocks();
}
function applyIncrement(colorThatJustMoved) {
    if (!timeControl) return;
    if (clockMs[colorThatJustMoved] > 0) {
        clockMs[colorThatJustMoved] += timeControl.increment;
    }
}
function onTimeOut(loserColor) {
    const winner = loserColor === 'w' ? 'Đen' : 'Trắng';
    showEndDialog('⏰', 'Hết giờ', `${winner} thắng do đối thủ hết giờ.`);
    setStatus(`Hết giờ. ${winner} thắng.`);
    gameOver = true;
    waitingBestmove = false;
    send({ type: 'stop' });
    updateButtons();
}

// ── End-of-game ─────────────────────────────────────────────────────

function checkAndAnnounceEnd() {
    if (game.isCheckmate()) {
        const winner = game.turn() === 'w' ? 'Đen' : 'Trắng';
        showEndDialog('♚', 'Chiếu hết', `${winner} thắng.`);
        gameOver = true; stopClock(); playMateSound(); updateStatus(); return true;
    }
    if (game.isStalemate()) {
        showEndDialog('🤝', 'Hòa', 'Hết nước đi (stalemate).');
        gameOver = true; stopClock(); updateStatus(); return true;
    }
    if (game.isInsufficientMaterial()) {
        showEndDialog('🤝', 'Hòa', 'Hai bên không đủ quân chiếu hết.');
        gameOver = true; stopClock(); updateStatus(); return true;
    }
    if (game.isThreefoldRepetition()) {
        showEndDialog('🤝', 'Hòa', 'Lặp lại vị trí ba lần.');
        gameOver = true; stopClock(); updateStatus(); return true;
    }
    if (game.isDraw()) {
        showEndDialog('🤝', 'Hòa', 'Luật 50 nước không bắt/đẩy tốt.');
        gameOver = true; stopClock(); updateStatus(); return true;
    }
    return false;
}

function showEndDialog(icon, title, msg) {
    endIconEl.textContent = icon;
    endTitleEl.textContent = title;
    endMsgEl.textContent = msg;
    if (typeof endDialog.showModal === 'function') endDialog.showModal();
}

$('btn-end-new').addEventListener('click', () => { endDialog.close(); newGame(); });
$('btn-end-close').addEventListener('click', () => endDialog.close());

// ── Hint ────────────────────────────────────────────────────────────

function requestHint() {
    if (!engineReady || waitingBestmove || gameOver || game.isGameOver()) return;
    if (mode === 'eve') return;
    if (mode === 'pve' && !isHumanTurn()) return;
    pendingHint = true;
    waitingBestmove = true;
    stopClock(); // pause clock during hint computation
    updateStatus();
    updateButtons();
    send({ type: 'go', movetimeMs: 500 });
}

function showHint(uci) {
    if (uci.length < 4) return;
    const from = uci.slice(0,2), to = uci.slice(2,4);
    document.querySelectorAll('.sq.hint').forEach(s => s.classList.remove('hint'));
    const f = sqEl(from), t = sqEl(to);
    if (f) f.classList.add('hint');
    if (t) t.classList.add('hint');
    setStatus(`Gợi ý: ${from} → ${to}`);
    setTimeout(() => {
        document.querySelectorAll('.sq.hint').forEach(s => s.classList.remove('hint'));
        if (!gameOver) updateStatus();
    }, 2700);
}

// ── Resign ───────────────────────────────────────────────────────────

function resignCurrentSide() {
    if (gameOver) return;
    if (mode === 'eve') return;
    const sideName = game.turn() === 'w' ? 'Trắng' : 'Đen';
    let title, msg;
    if (mode === 'pve') {
        const humanColor = humanSideEl.value === 'white' ? 'w' : 'b';
        if (game.turn() !== humanColor) return; // engine doesn't resign
        if (!confirm('Bạn xác nhận đầu hàng?')) return;
        const winnerName = humanColor === 'w' ? 'Đen' : 'Trắng';
        title = 'Đầu hàng';
        msg = `Bạn đầu hàng. Máy (${winnerName}) thắng.`;
    } else { // pvp
        if (!confirm(`${sideName} đầu hàng?`)) return;
        const winnerName = sideName === 'Trắng' ? 'Đen' : 'Trắng';
        title = 'Đầu hàng';
        msg = `${sideName} đầu hàng. ${winnerName} thắng.`;
    }
    gameOver = true;
    stopClock();
    waitingBestmove = false;
    pendingHint = false;
    send({ type: 'stop' });
    showEndDialog('🏳️', title, msg);
    setStatus(msg);
    updateButtons();
}

// ── Sound (WebAudio) ────────────────────────────────────────────────

let audioCtx = null;
let soundOn = localStorage.getItem('mychess.sound') !== 'off';

function syncSoundButton() {
    btnSound.textContent = soundOn ? '🔔' : '🔕';
    btnSound.classList.toggle('muted', !soundOn);
}

function ensureAudio() {
    if (!soundOn) return null;
    try {
        if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        if (audioCtx.state === 'suspended') audioCtx.resume();
        return audioCtx;
    } catch (_) { return null; }
}

function tone(freq, duration = 80, type = 'sine', vol = 0.13, delay = 0) {
    const ctx = ensureAudio();
    if (!ctx) return;
    const t0 = ctx.currentTime + delay / 1000;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = type;
    osc.frequency.setValueAtTime(freq, t0);
    gain.gain.setValueAtTime(vol, t0);
    gain.gain.exponentialRampToValueAtTime(0.0001, t0 + duration / 1000);
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start(t0);
    osc.stop(t0 + duration / 1000 + 0.01);
}

function playMoveSound(result) {
    if (!result) return;
    if (game.inCheck()) { playCheckSound(); return; }
    const flags = result.flags || '';
    if (result.captured || flags.includes('e')) {
        tone(220, 60, 'triangle', 0.12);
        tone(330, 90, 'triangle', 0.10, 50);
    } else if (flags.includes('k') || flags.includes('q')) {
        tone(330, 60, 'sine', 0.12);
        tone(440, 90, 'sine', 0.10, 60);
    } else {
        tone(330, 60, 'sine', 0.10);
    }
}
function playCheckSound() {
    tone(880, 100, 'square', 0.10);
    tone(660, 130, 'square', 0.08, 110);
}
function playMateSound() {
    tone(220, 200, 'sawtooth', 0.12);
    tone(180, 280, 'sawtooth', 0.12, 200);
}
function playStartSound() {
    tone(523, 70, 'sine', 0.08);
    tone(659, 70, 'sine', 0.08, 70);
    tone(784, 110, 'sine', 0.08, 140);
}

btnSound.addEventListener('click', () => {
    soundOn = !soundOn;
    localStorage.setItem('mychess.sound', soundOn ? 'on' : 'off');
    syncSoundButton();
    if (soundOn) tone(660, 60); // confirm
});
syncSoundButton();

// ── Drag and drop / click ───────────────────────────────────────────

boardEl.addEventListener('click', (ev) => {
    const sqDiv = ev.target.closest('.sq');
    if (!sqDiv) return;
    if (viewIdx !== -1) { viewIdx = -1; renderPosition(); renderHistory(); return; }
    if (gameOver) return;
    if (!isHumanTurn() || waitingBestmove) return;
    const sq = sqDiv.dataset.sq;
    const piece = game.get(sq);

    if (selected) {
        if (sq === selected) { clearSelection(); return; }
        if (piece && piece.color === game.turn()) { selectSquare(sq); return; }
        tryUserMove(selected, sq);
        return;
    }
    if (piece && piece.color === game.turn()) selectSquare(sq);
});

boardEl.addEventListener('dragstart', (ev) => {
    if (viewIdx !== -1 || gameOver) { ev.preventDefault(); return; }
    if (!isHumanTurn() || waitingBestmove) { ev.preventDefault(); return; }
    const piece = ev.target.closest('.piece');
    if (!piece) return;
    const sq = piece.dataset.sq;
    const p = game.get(sq);
    if (!p || p.color !== game.turn()) { ev.preventDefault(); return; }
    selectSquare(sq);
    ev.dataTransfer.setData('text/plain', sq);
    piece.classList.add('dragging');
});
boardEl.addEventListener('dragend', () => {
    document.querySelectorAll('.piece.dragging').forEach(p => p.classList.remove('dragging'));
});
boardEl.addEventListener('dragover', (ev) => { ev.preventDefault(); });
boardEl.addEventListener('drop', (ev) => {
    ev.preventDefault();
    const target = ev.target.closest('.sq');
    if (!target) return;
    const from = ev.dataTransfer.getData('text/plain');
    if (!from) return;
    tryUserMove(from, target.dataset.sq);
});

// ── Buttons / selects ───────────────────────────────────────────────

$('btn-new').addEventListener('click', newGame);
btnUndo.addEventListener('click', undoMove);
$('btn-flip').addEventListener('click', () => {
    flipped = !flipped;
    buildBoard();
    if (board3d) board3d.setFlipped(flipped);
    renderPosition();
    renderPlayers();
    renderClocks();
});

function handleSquareClick3D(sq) {
    if (viewIdx !== -1) { viewIdx = -1; renderPosition(); renderHistory(); return; }
    if (gameOver) return;
    if (!isHumanTurn() || waitingBestmove) return;
    const piece = game.get(sq);
    if (selected) {
        if (sq === selected) { clearSelection(); return; }
        if (piece && piece.color === game.turn()) { selectSquare(sq); return; }
        tryUserMove(selected, sq);
        return;
    }
    if (piece && piece.color === game.turn()) selectSquare(sq);
}

function ensure3D() {
    if (board3d) return;
    const c = $('board-wrap-3d');
    c.style.display = '';
    board3d = new Board3D(c, { onSquareClick: handleSquareClick3D });
    board3d.setFlipped(flipped);
}

function applyViewMode() {
    const wrap2d = $('board-wrap-2d');
    const wrap3d = $('board-wrap-3d');
    const btn = $('btn-view3d');
    if (view3D) {
        try {
            ensure3D();
        } catch (err) {
            console.error('[3D] init failed:', err);
            setStatus('Không khởi tạo được 3D: ' + (err && err.message ? err.message : err));
            view3D = false;
            localStorage.setItem('mychess.view3d', 'off');
            wrap2d.style.display = '';
            wrap3d.style.display = 'none';
            btn.textContent = '3D';
            return;
        }
        wrap2d.style.display = 'none';
        wrap3d.style.display = '';
        board3d.resume();
        syncBoard3D();
        btn.textContent = '2D';
    } else {
        wrap2d.style.display = '';
        wrap3d.style.display = 'none';
        if (board3d) board3d.pause();
        btn.textContent = '3D';
    }
}

$('btn-view3d').addEventListener('click', () => {
    view3D = !view3D;
    localStorage.setItem('mychess.view3d', view3D ? 'on' : 'off');
    applyViewMode();
});
$('btn-reset-view').addEventListener('click', () => {
    if (board3d) board3d.resetView();
});
$('btn-stop').addEventListener('click', () => {
    send({ type: 'stop' });
    waitingBestmove = false;
    pendingHint = false;
    updateStatus();
    updateButtons();
});
btnHint.addEventListener('click', requestHint);
btnResign.addEventListener('click', resignCurrentSide);
$('btn-reconnect').addEventListener('click', () => {
    reconnectAttempts = 0;
    resumingSession = true;
    $('btn-reconnect').style.display = 'none';
    setStatus('Đang kết nối lại…');
    connect();
});

document.addEventListener('visibilitychange', () => {
    if (document.visibilityState !== 'visible') return;
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;
    reconnectAttempts = 0;
    resumingSession = true;
    $('btn-reconnect').style.display = 'none';
    setStatus('Đang kết nối lại…');
    connect();
});

$('btn-fen-copy').addEventListener('click', async () => {
    try { await navigator.clipboard.writeText(game.fen()); setStatus('Đã copy FEN.'); }
    catch (_) { fenEl.select(); document.execCommand('copy'); }
});
$('btn-fen-load').addEventListener('click', () => {
    const fen = fenEl.value.trim();
    try {
        new Chess(fen);
        game.load(fen);
        historySAN = [];
        lastMove = null;
        viewIdx = -1;
        gameOver = false;
        clearInfo();
        setupClockFromSelect();
        renderPosition();
        renderHistory();
        renderPlayers();
        renderClocks();
        send({ type: 'newgame', fen });
        updateStatus();
        updateButtons();
        if (mode !== 'pvp') setTimeout(maybeTriggerEngine, 50);
    } catch (e) {
        setStatus('FEN không hợp lệ.');
    }
});

modeEl.addEventListener('change', () => {
    mode = modeEl.value;
    rowSide.style.display = (mode === 'pve') ? '' : 'none';
    rowLevel.style.display = (mode === 'pvp') ? 'none' : '';
    renderPlayers();
    updateStatus();
    updateButtons();
    setTimeout(maybeTriggerEngine, 50);
});
humanSideEl.addEventListener('change', () => {
    flipped = humanSideEl.value === 'black';
    buildBoard();
    renderPosition();
    renderPlayers();
    renderClocks();
    setTimeout(maybeTriggerEngine, 50);
});
levelEl.addEventListener('change', () => {
    if (levelEl.value === 'custom') {
        rowThinktime.style.display = '';
    } else {
        rowThinktime.style.display = 'none';
        thinktimeEl.value = levelEl.value;
        thinktimeLabel.textContent = thinktimeEl.value;
    }
    renderPlayers();
});
thinktimeEl.addEventListener('input', () => {
    thinktimeLabel.textContent = thinktimeEl.value;
});
timeControlEl.addEventListener('change', () => {
    setupClockFromSelect();
    if (!gameOver && historySAN.length > 0 && timeControl) {
        // If a game is in progress and a side is currently to move, restart the clock.
        startClock();
    }
});

function newGame() {
    send({ type: 'stop' });
    waitingBestmove = false;
    pendingHint = false;
    gameOver = false;
    game.reset();
    historySAN = [];
    lastMove = null;
    selected = null; legalForSelected = [];
    viewIdx = -1;
    clearInfo();
    setupClockFromSelect();
    renderPosition();
    renderHistory();
    renderPlayers();
    renderClocks();
    sendNewGame();
    updateStatus();
    updateButtons();
    playStartSound();
    // Start clock now (white's clock begins ticking on game start).
    if (timeControl) startClock();
    setTimeout(maybeTriggerEngine, 50);
}

function undoMove() {
    if (!historySAN.length || gameOver) return;
    let count = 1;
    if (mode === 'pve') {
        const humanColor = humanSideEl.value === 'white' ? 'w' : 'b';
        if (waitingBestmove) {
            send({ type: 'stop' });
            waitingBestmove = false;
            pendingHint = false;
        }
        const sim = new Chess(game.fen());
        if (sim.turn() !== humanColor) count = 2;
    }
    for (let i = 0; i < count && historySAN.length; i++) {
        game.undo();
        historySAN.pop();
    }
    lastMove = null;
    selected = null; legalForSelected = [];
    viewIdx = -1;
    clearInfo();
    renderPosition();
    renderHistory();
    renderClocks();
    syncEngine();
    updateStatus();
    updateButtons();
    setTimeout(maybeTriggerEngine, 50);
}

// ── Button enable/disable ───────────────────────────────────────────

function updateButtons() {
    const over = gameOver || game.isGameOver();
    btnResign.disabled = over || mode === 'eve';
    if (mode === 'pve') {
        btnResign.disabled = btnResign.disabled || !isHumanTurn();
    }
    btnHint.disabled = over || waitingBestmove || mode === 'eve' ||
                       (mode === 'pve' && !isHumanTurn()) ||
                       !engineReady;
    btnUndo.disabled = over || !historySAN.length;
}

// ── Init ────────────────────────────────────────────────────────────

mode = modeEl.value;
rowSide.style.display = (mode === 'pve') ? '' : 'none';
rowLevel.style.display = (mode === 'pvp') ? 'none' : '';
buildBoard();
renderPosition();
renderPlayers();
setupClockFromSelect();
updateStatus();
updateButtons();
applyViewMode();
connect();
