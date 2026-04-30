// MyChess web GUI — entry point.
//
// Responsibilities kept here (glue):
//   • WebSocket lifecycle (connect, reconnect, message dispatch).
//   • Game flow: user/engine move application, undo, new game.
//   • Engine config (Elo → movetime + blunder probability).
//   • Top-level button/select event bindings.
//   • Init: build board, load personas, connect.
//
// Everything else lives in dedicated modules; see imports below.

import { Chess } from './vendor/chess.js';
import { Board3D } from './board3d.js';
import { state, send, MAX_RECONNECT } from './state.js';
import {
    buildBoard,
    renderPosition,
    syncBoard3D,
    renderHistory,
    renderInfo,
    clearInfo,
    renderPlayers,
    askPromotion,
    setStatus,
    updateStatus,
    sqEl,
    canDragLive,
    sideToMoveLive,
    isHumanTurn,
    currentEloValue,
} from './board-render.js';
import {
    setupClockFromSelect,
    startClock,
    stopClock,
    renderClocks,
    applyIncrement,
    setOnTimeOutHandler,
} from './clock.js';
import {
    playMoveSound,
    playMateSound,
    playStartSound,
} from './sound.js';
import {
    applyChatPanelState,
    refreshChatInputState,
    appendChatMsg,
    clearChatLog,
    setSendChatHandler,
} from './chat-panel.js';
import {
    loadPersonas,
    triggerPersonaChat,
    sendUserChat,
    resolveChatMessage,
    setEloLabelHandler,
} from './persona-chat.js';
import { initFeatureFlagsUI } from './feature-flags.js';
import {
    updateMoodFromEval,
    resetMood,
} from './persona-mood.js';
import {
    requestPrecompute,
    handlePrecomputeResponse,
    resetPrecompute,
    compareUserMove,
    shouldPrecomputeForPersona,
} from './persona-precompute.js';

// ── DOM refs we still need at the entry level ──────────────────────

const $ = (id) => document.getElementById(id);
const boardEl       = $('board');
const fenEl         = $('fen');
const modeEl        = $('mode');
const humanSideEl   = $('human-side');
const eloSliderEl   = $('elo-slider');
const eloLabelEl    = $('elo-label');
const eloDetailEl   = $('elo-detail');
const rowSide       = $('row-side');
const rowLevel      = $('row-level');
const rowPersona    = $('row-persona');
const rowEloDetail  = $('elo-row');
const endDialog     = $('end-dialog');
const endTitleEl    = $('end-title');
const endMsgEl      = $('end-msg');
const endIconEl     = $('end-icon');
const btnHint       = $('btn-hint');
const btnResign     = $('btn-resign');
const btnUndo       = $('btn-undo');

// ── WebSocket ──────────────────────────────────────────────────────

const wsUrl = (location.protocol === 'https:' ? 'wss:' : 'ws:') + '//' + location.host;

function connect() {
    if (state.reconnectTimer) { clearTimeout(state.reconnectTimer); state.reconnectTimer = null; }
    const reconnectBtn = $('btn-reconnect');
    state.ws = new WebSocket(wsUrl);
    state.ws.addEventListener('open', () => {
        state.reconnectAttempts = 0;
        if (reconnectBtn) reconnectBtn.style.display = 'none';
        setStatus(state.resumingSession ? 'Đang khôi phục phiên…' : 'Đang khởi tạo máy…');
    });
    state.ws.addEventListener('close', () => {
        state.engineReady = false;
        state.waitingBestmove = false;
        if (state.reconnectAttempts < MAX_RECONNECT) {
            state.reconnectAttempts++;
            const delay = Math.min(8000, 1000 * Math.pow(2, state.reconnectAttempts - 1));
            setStatus(`Mất kết nối. Thử lại ${state.reconnectAttempts}/${MAX_RECONNECT} sau ${Math.round(delay/1000)}s…`);
            state.resumingSession = true;
            state.reconnectTimer = setTimeout(connect, delay);
        } else {
            setStatus('Mất kết nối máy. Bấm "Kết nối lại" để thử lại.');
            if (reconnectBtn) reconnectBtn.style.display = '';
        }
    });
    state.ws.addEventListener('error', () => { /* close fires too — handled there */ });
    state.ws.addEventListener('message', (ev) => {
        let msg;
        try { msg = JSON.parse(ev.data); } catch (_) { return; }
        handleServerMessage(msg);
    });
}

function handleServerMessage(msg) {
    switch (msg.type) {
        case 'ready':
            state.engineReady = true;
            if (state.resumingSession) {
                state.resumingSession = false;
                syncEngine();
            } else {
                sendNewGame();
            }
            updateStatus();
            updateButtons();
            refreshChatInputState();
            maybeTriggerEngine();
            break;
        case 'info':
            renderInfo(msg);
            updateMoodFromEval(msg);
            break;
        case 'bestmove':
            state.waitingBestmove = false;
            clearInfo();
            if (state.pendingHint) {
                state.pendingHint = false;
                if (msg.uci) showHint(msg.uci);
                if (state.timeControl && !state.gameOver) startClock();
                updateStatus();
                updateButtons();
                return;
            }
            if (msg.uci) applyEngineMove(msg.uci);
            else { updateStatus(); updateButtons(); }
            break;
        case 'chat':
            resolveChatMessage(msg);
            break;
        case 'precompute':
            handlePrecomputeResponse(msg);
            break;
        case 'error':
            setStatus('Máy báo lỗi: ' + msg.msg);
            break;
    }
}

// ── Engine config (Elo → movetime + blunder probability) ───────────
// Engine MyChess has no UCI Skill option, so weakening is purely external.

function eloConfig(elo) {
    // Engine baseline ~2200 ELO ở 1000ms/move (xem plans/elo_measurements.md).
    // Không có UCI Skill → weakening = giảm movetime + random blunder.
    if (elo <= 600)  return { movetimeMs: 10,   blunderProb: 0.80  };
    if (elo <= 800)  return { movetimeMs: 20,   blunderProb: 0.55  };
    if (elo <= 1000) return { movetimeMs: 30,   blunderProb: 0.40  };
    if (elo <= 1200) return { movetimeMs: 60,   blunderProb: 0.25  };
    if (elo <= 1400) return { movetimeMs: 150,  blunderProb: 0.12  };
    if (elo <= 1600) return { movetimeMs: 400,  blunderProb: 0.05  };
    if (elo <= 1800) return { movetimeMs: 1000, blunderProb: 0.015 };
    if (elo <= 2000) return { movetimeMs: 2500, blunderProb: 0     };
    return                  { movetimeMs: 5000, blunderProb: 0     };
}
function currentMovetimeMs()  { return eloConfig(currentEloValue()).movetimeMs; }
function currentBlunderProb() { return eloConfig(currentEloValue()).blunderProb; }

function renderEloLabel() {
    const elo = currentEloValue();
    const cfg = eloConfig(elo);
    eloLabelEl.textContent = `Elo: ${elo}`;
    const blunderPct = (cfg.blunderProb * 100).toFixed(cfg.blunderProb < 0.01 ? 1 : 0);
    eloDetailEl.textContent = `~${cfg.movetimeMs}ms · blunder ${blunderPct}%`;
}

function sendNewGame() { send({ type: 'newgame' }); }

function syncEngine() {
    const uciHistory = state.game.history({ verbose: true })
        .map(m => m.from + m.to + (m.promotion || ''));
    send({ type: 'position', moves: uciHistory });
}

function maybeTriggerEngine() {
    if (!state.engineReady) return;
    if (state.gameOver) return;
    if (state.game.isGameOver()) return;
    if (state.mode === 'pvp') return;
    if (state.mode === 'pve' && isHumanTurn()) return;
    state.waitingBestmove = true;
    if (state.timeControl) startClock();
    updateStatus();
    updateButtons();
    send({ type: 'go', movetimeMs: currentMovetimeMs() });
}

// ── Move flow ──────────────────────────────────────────────────────

function selectSquare(sq) {
    state.selected = sq;
    state.legalForSelected = state.game.moves({ square: sq, verbose: true });
    renderPosition();
}
function clearSelection() {
    state.selected = null;
    state.legalForSelected = [];
    renderPosition();
}

async function tryUserMove(from, to) {
    const candidates = state.game.moves({ square: from, verbose: true })
        .filter(m => m.to === to);
    if (!candidates.length) { clearSelection(); return false; }

    let promo = undefined;
    if (candidates.some(m => m.promotion)) {
        promo = await askPromotion();
        if (!promo) { clearSelection(); return false; }
    }

    // Lưu FEN trước khi đi để precompute compare hậu kiểm.
    const fenBefore = state.game.fen();
    const uciPlayed = from + to + (promo || '');

    let result;
    try { result = state.game.move({ from, to, promotion: promo }); }
    catch (_) { result = null; }
    if (!result) { clearSelection(); return false; }

    cancelIdleTimer();
    const moveQuality = compareUserMove(uciPlayed, fenBefore);

    onMoveDone(result);
    triggerPersonaChat('user_move', { san: result.san, result, moveQuality });
    return true;
}

function onMoveDone(result) {
    state.selected = null; state.legalForSelected = [];
    state.lastMove = { from: result.from, to: result.to };
    state.historySAN.push(result.san);
    state.viewIdx = -1;
    state.plyCount++;

    applyIncrement(result.color);
    playMoveSound(result);

    renderPosition({ animatedMove: result });
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

    const blunderP = currentBlunderProb();
    if (blunderP > 0 && Math.random() < blunderP) {
        const legal = state.game.moves({ verbose: true });
        if (legal.length > 1) {
            const pick = legal[Math.floor(Math.random() * legal.length)];
            uci = pick.from + pick.to + (pick.promotion || '');
        }
    }

    const from = uci.slice(0, 2), to = uci.slice(2, 4);
    const promo = uci.length >= 5 ? uci[4] : undefined;
    let result;
    try { result = state.game.move({ from, to, promotion: promo }); }
    catch (_) { result = null; }
    if (!result) {
        setStatus('Máy trả về nước không hợp lệ: ' + uci);
        return;
    }
    onMoveDone(result);
    triggerPersonaChat('engine_move', { san: result.san, uci, result });
    // Phase 3: tới lượt user → spawn precompute để biết top-1 nước user nên đi.
    // Đồng thời start idle timer cho "tiên tri" nếu user nghĩ lâu.
    if (!state.gameOver && shouldPrecomputeForPersona(state.currentPersona)) {
        // Delay nhỏ cho UI settle trước khi gửi command engine.
        setTimeout(() => requestPrecompute(), 100);
        scheduleIdleTimer();
    }
}

// ── Idle timer cho precognition (Phase 3) ──────────────────────────

const IDLE_DELAY_MS = 6000;       // sau N giây user chưa đi → có cơ hội
const PRECOG_BASE_PROB = 0.20;    // xác suất base, scale theo chattiness

function scheduleIdleTimer() {
    cancelIdleTimer();
    state.idleTimerHandle = setTimeout(() => {
        state.idleTimerHandle = null;
        if (state.gameOver) return;
        if (!isHumanTurn()) return;
        if (!state.currentPersona) return;
        const chat = state.currentPersona.chattiness || 0.5;
        if (Math.random() > PRECOG_BASE_PROB * chat) return;
        triggerPersonaChat('precognition', {});
    }, IDLE_DELAY_MS);
}

function cancelIdleTimer() {
    if (state.idleTimerHandle) {
        clearTimeout(state.idleTimerHandle);
        state.idleTimerHandle = null;
    }
}

// ── End-of-game ────────────────────────────────────────────────────

function personaSideColor() {
    if (state.mode !== 'pve' || !state.currentPersona) return null;
    return humanSideEl.value === 'white' ? 'b' : 'w';
}

function announceEnd(icon, title, msg, outcome) {
    showEndDialog(icon, title, msg);
    state.gameOver = true; stopClock(); updateStatus();
    if (outcome === 'mate') playMateSound();
    const personaColor = personaSideColor();
    if (personaColor) {
        let evt = 'draw';
        if (outcome === 'mate') {
            const losingColor = state.game.turn();
            evt = (losingColor === personaColor) ? 'lose' : 'win';
        }
        triggerPersonaChat(evt);
    }
}

function checkAndAnnounceEnd() {
    if (state.game.isCheckmate()) {
        const winner = state.game.turn() === 'w' ? 'Đen' : 'Trắng';
        announceEnd('♚', 'Chiếu hết', `${winner} thắng.`, 'mate');
        return true;
    }
    if (state.game.isStalemate()) {
        announceEnd('🤝', 'Hòa', 'Hết nước đi (stalemate).', 'draw');
        return true;
    }
    if (state.game.isInsufficientMaterial()) {
        announceEnd('🤝', 'Hòa', 'Hai bên không đủ quân chiếu hết.', 'draw');
        return true;
    }
    if (state.game.isThreefoldRepetition()) {
        announceEnd('🤝', 'Hòa', 'Lặp lại vị trí ba lần.', 'draw');
        return true;
    }
    if (state.game.isDraw()) {
        announceEnd('🤝', 'Hòa', 'Luật 50 nước không bắt/đẩy tốt.', 'draw');
        return true;
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

// ── Hint ───────────────────────────────────────────────────────────

function requestHint() {
    if (!state.engineReady || state.waitingBestmove || state.gameOver || state.game.isGameOver()) return;
    if (state.mode === 'eve') return;
    if (state.mode === 'pve' && !isHumanTurn()) return;
    state.pendingHint = true;
    state.waitingBestmove = true;
    stopClock();
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
        if (!state.gameOver) updateStatus();
    }, 2700);
}

// ── Resign ─────────────────────────────────────────────────────────

function resignCurrentSide() {
    if (state.gameOver) return;
    if (state.mode === 'eve') return;
    const sideName = state.game.turn() === 'w' ? 'Trắng' : 'Đen';
    let title, msg;
    if (state.mode === 'pve') {
        const humanColor = humanSideEl.value === 'white' ? 'w' : 'b';
        if (state.game.turn() !== humanColor) return;
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
    state.gameOver = true;
    stopClock();
    state.waitingBestmove = false;
    state.pendingHint = false;
    send({ type: 'stop' });
    showEndDialog('🏳️', title, msg);
    setStatus(msg);
    updateButtons();
}

// ── Time-out hook (clock fires, app.js handles end-of-game) ────────

setOnTimeOutHandler((loserColor) => {
    const winner = loserColor === 'w' ? 'Đen' : 'Trắng';
    showEndDialog('⏰', 'Hết giờ', `${winner} thắng do đối thủ hết giờ.`);
    setStatus(`Hết giờ. ${winner} thắng.`);
    state.gameOver = true;
    state.waitingBestmove = false;
    send({ type: 'stop' });
    updateButtons();
});

// Cross-module callbacks: persona-chat + chat-panel
setEloLabelHandler(renderEloLabel);
setSendChatHandler(sendUserChat);

// ── Click / drag handlers on board ─────────────────────────────────

boardEl.addEventListener('click', (ev) => {
    const sqDiv = ev.target.closest('.sq');
    if (!sqDiv) return;
    if (state.viewIdx !== -1) { state.viewIdx = -1; renderPosition(); renderHistory(); return; }
    if (state.gameOver) return;
    if (!isHumanTurn() || state.waitingBestmove) return;
    const sq = sqDiv.dataset.sq;
    const piece = state.game.get(sq);

    if (state.selected) {
        if (sq === state.selected) { clearSelection(); return; }
        if (piece && piece.color === state.game.turn()) { selectSquare(sq); return; }
        tryUserMove(state.selected, sq);
        return;
    }
    if (piece && piece.color === state.game.turn()) selectSquare(sq);
});

boardEl.addEventListener('dragstart', (ev) => {
    if (state.viewIdx !== -1 || state.gameOver) { ev.preventDefault(); return; }
    if (!isHumanTurn() || state.waitingBestmove) { ev.preventDefault(); return; }
    const piece = ev.target.closest('.piece');
    if (!piece) return;
    const sq = piece.dataset.sq;
    const p = state.game.get(sq);
    if (!p || p.color !== state.game.turn()) { ev.preventDefault(); return; }
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

function handleSquareClick3D(sq) {
    if (state.viewIdx !== -1) { state.viewIdx = -1; renderPosition(); renderHistory(); return; }
    if (state.gameOver) return;
    if (!isHumanTurn() || state.waitingBestmove) return;
    const piece = state.game.get(sq);
    if (state.selected) {
        if (sq === state.selected) { clearSelection(); return; }
        if (piece && piece.color === state.game.turn()) { selectSquare(sq); return; }
        tryUserMove(state.selected, sq);
        return;
    }
    if (piece && piece.color === state.game.turn()) selectSquare(sq);
}

// ── 3D view toggle ─────────────────────────────────────────────────

function ensure3D() {
    if (state.board3d) return;
    const c = $('board-wrap-3d');
    c.style.display = '';
    state.board3d = new Board3D(c, { onSquareClick: handleSquareClick3D });
    state.board3d.setFlipped(state.flipped);
}

function applyViewMode() {
    const wrap2d = $('board-wrap-2d');
    const wrap3d = $('board-wrap-3d');
    const btn = $('btn-view3d');
    if (state.view3D) {
        try {
            ensure3D();
        } catch (err) {
            console.error('[3D] init failed:', err);
            setStatus('Không khởi tạo được 3D: ' + (err && err.message ? err.message : err));
            state.view3D = false;
            localStorage.setItem('mychess.view3d', 'off');
            wrap2d.style.display = '';
            wrap3d.style.display = 'none';
            btn.textContent = '3D';
            return;
        }
        wrap2d.style.display = 'none';
        wrap3d.style.display = '';
        state.board3d.resume();
        syncBoard3D();
        btn.textContent = '2D';
    } else {
        wrap2d.style.display = '';
        wrap3d.style.display = 'none';
        if (state.board3d) state.board3d.pause();
        btn.textContent = '3D';
    }
}

$('btn-view3d').addEventListener('click', () => {
    state.view3D = !state.view3D;
    localStorage.setItem('mychess.view3d', state.view3D ? 'on' : 'off');
    applyViewMode();
});
$('btn-reset-view').addEventListener('click', () => {
    if (state.board3d) state.board3d.resetView();
});

// ── Buttons / selects ──────────────────────────────────────────────

$('btn-new').addEventListener('click', newGame);
btnUndo.addEventListener('click', undoMove);
$('btn-flip').addEventListener('click', () => {
    state.flipped = !state.flipped;
    buildBoard();
    if (state.board3d) state.board3d.setFlipped(state.flipped);
    renderPosition();
    renderPlayers();
    renderClocks();
});
$('btn-stop').addEventListener('click', () => {
    send({ type: 'stop' });
    state.waitingBestmove = false;
    state.pendingHint = false;
    updateStatus();
    updateButtons();
});
btnHint.addEventListener('click', requestHint);
btnResign.addEventListener('click', resignCurrentSide);
$('btn-reconnect').addEventListener('click', () => {
    state.reconnectAttempts = 0;
    state.resumingSession = true;
    $('btn-reconnect').style.display = 'none';
    setStatus('Đang kết nối lại…');
    connect();
});

document.addEventListener('visibilitychange', () => {
    if (document.visibilityState !== 'visible') return;
    if (state.ws && (state.ws.readyState === WebSocket.OPEN || state.ws.readyState === WebSocket.CONNECTING)) return;
    state.reconnectAttempts = 0;
    state.resumingSession = true;
    $('btn-reconnect').style.display = 'none';
    setStatus('Đang kết nối lại…');
    connect();
});

$('btn-fen-copy').addEventListener('click', async () => {
    try { await navigator.clipboard.writeText(state.game.fen()); setStatus('Đã copy FEN.'); }
    catch (_) { fenEl.select(); document.execCommand('copy'); }
});
$('btn-fen-load').addEventListener('click', () => {
    const fen = fenEl.value.trim();
    try {
        new Chess(fen);
        state.game.load(fen);
        state.historySAN = [];
        state.lastMove = null;
        state.viewIdx = -1;
        state.gameOver = false;
        state.plyCount = 0;
        state.lastChatPly = -10;
        state.chatHistory = [];
        clearChatLog();
        appendChatMsg('system', '— Đã nạp FEN, bắt đầu lại cuộc trò chuyện —');
        refreshChatInputState();
        clearInfo();
        setupClockFromSelect();
        renderPosition();
        renderHistory();
        renderPlayers();
        renderClocks();
        send({ type: 'newgame', fen });
        updateStatus();
        updateButtons();
        if (state.mode !== 'pvp') setTimeout(maybeTriggerEngine, 50);
    } catch (e) {
        setStatus('FEN không hợp lệ.');
    }
});

modeEl.addEventListener('change', () => {
    state.mode = modeEl.value;
    rowSide.style.display     = (state.mode === 'pve') ? '' : 'none';
    rowLevel.style.display    = (state.mode === 'pvp') ? 'none' : '';
    rowEloDetail.style.display= (state.mode === 'pvp') ? 'none' : '';
    rowPersona.style.display  = (state.mode === 'pve') ? '' : 'none';
    refreshChatInputState();
    renderPlayers();
    updateStatus();
    updateButtons();
    setTimeout(maybeTriggerEngine, 50);
});
humanSideEl.addEventListener('change', () => {
    state.flipped = humanSideEl.value === 'black';
    buildBoard();
    renderPosition();
    renderPlayers();
    renderClocks();
    setTimeout(maybeTriggerEngine, 50);
});
eloSliderEl.addEventListener('input', () => {
    renderEloLabel();
    renderPlayers();
});

// ── newGame / undoMove ─────────────────────────────────────────────

function newGame() {
    send({ type: 'stop' });
    state.waitingBestmove = false;
    state.pendingHint = false;
    state.gameOver = false;
    state.game.reset();
    state.historySAN = [];
    state.lastMove = null;
    state.selected = null;
    state.legalForSelected = [];
    state.viewIdx = -1;
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
    state.plyCount = 0;
    state.lastChatPly = -10;
    state.chatHistory = [];
    state.recentReplies = [];
    state.lastMoveQuality = null;
    resetMood();
    resetPrecompute();
    cancelIdleTimer();
    clearChatLog();
    if (state.mode === 'pve' && state.currentPersona) {
        appendChatMsg('system', `— Ván mới · đối thủ ${state.currentPersona.emoji} ${state.currentPersona.name} —`);
    } else {
        appendChatMsg('system', '— Ván mới —');
    }
    refreshChatInputState();
    triggerPersonaChat('start');
    if (state.timeControl) startClock();
    setTimeout(maybeTriggerEngine, 50);
}

function undoMove() {
    if (!state.historySAN.length || state.gameOver) return;
    let count = 1;
    if (state.mode === 'pve') {
        const humanColor = humanSideEl.value === 'white' ? 'w' : 'b';
        if (state.waitingBestmove) {
            send({ type: 'stop' });
            state.waitingBestmove = false;
            state.pendingHint = false;
        }
        const sim = new Chess(state.game.fen());
        if (sim.turn() === humanColor) count = 2;
    }
    for (let i = 0; i < count && state.historySAN.length; i++) {
        state.game.undo();
        state.historySAN.pop();
    }
    state.lastMove = null;
    state.selected = null;
    state.legalForSelected = [];
    state.viewIdx = -1;
    clearInfo();
    renderPosition();
    renderHistory();
    renderClocks();
    syncEngine();
    updateStatus();
    updateButtons();
    setTimeout(maybeTriggerEngine, 50);
}

// ── Button enable/disable ──────────────────────────────────────────

function updateButtons() {
    const over = state.gameOver || state.game.isGameOver();
    btnResign.disabled = over || state.mode === 'eve';
    if (state.mode === 'pve') {
        btnResign.disabled = btnResign.disabled || !isHumanTurn();
    }
    btnHint.disabled = over || state.waitingBestmove || state.mode === 'eve' ||
                       (state.mode === 'pve' && !isHumanTurn()) ||
                       !state.engineReady;
    btnUndo.disabled = over || !state.historySAN.length;
}

// ── Init ───────────────────────────────────────────────────────────

state.mode = modeEl.value;
rowSide.style.display      = (state.mode === 'pve') ? '' : 'none';
rowLevel.style.display     = (state.mode === 'pvp') ? 'none' : '';
rowEloDetail.style.display = (state.mode === 'pvp') ? 'none' : '';
rowPersona.style.display   = (state.mode === 'pve') ? '' : 'none';
buildBoard();
renderPosition();
renderEloLabel();
renderPlayers();
setupClockFromSelect();
updateStatus();
updateButtons();
applyViewMode();
applyChatPanelState();
refreshChatInputState();
initFeatureFlagsUI('feature-flags');
loadPersonas();
connect();
