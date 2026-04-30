// Pure rendering of the 2D board, history list, engine info pane, player bar,
// and the promotion modal. Functions read from `state` and write to the DOM.
import { state, FILES } from './state.js';
import { Chess } from './vendor/chess.js';

const boardEl       = document.getElementById('board');
const filesTop      = document.getElementById('files-top');
const filesBottom   = document.getElementById('files-bottom');
const ranksLeft     = document.getElementById('ranks-left');
const ranksRight    = document.getElementById('ranks-right');
const fenEl         = document.getElementById('fen');
const histEl        = document.getElementById('history');
const statusEl      = document.getElementById('status');
const infoDepth     = document.getElementById('info-depth');
const infoScore     = document.getElementById('info-score');
const infoNodes     = document.getElementById('info-nodes');
const infoNps       = document.getElementById('info-nps');
const infoPv        = document.getElementById('info-pv');
const playerTopName    = document.getElementById('player-top-name');
const playerBottomName = document.getElementById('player-bottom-name');
const playerTopIcon    = document.getElementById('player-top-icon');
const playerBottomIcon = document.getElementById('player-bottom-icon');
const humanSideEl   = document.getElementById('human-side');
const eloSliderEl   = document.getElementById('elo-slider');
const promoOverlay  = document.getElementById('promo-overlay');

// ── Tiny game-state readers (used both by render and by app.js flow) ──

export function currentEloValue() {
    return parseInt(eloSliderEl.value, 10) || 1400;
}
// Elo is intentionally hidden from the UI; persona name alone identifies opponent.
export function levelLabel() { return ''; }
export function topPlayerColor() {
    return state.flipped ? 'w' : 'b';
}
export function sideToMoveLive() { return state.game.turn(); }
export function canDragLive() {
    if (state.gameOver) return false;
    if (state.mode === 'eve') return false;
    if (state.waitingBestmove) return false;
    if (state.game.isGameOver()) return false;
    return true;
}
export function isHumanTurn() {
    if (state.mode === 'pvp') return true;
    if (state.mode === 'eve') return false;
    const humanColor = humanSideEl.value === 'white' ? 'w' : 'b';
    return state.game.turn() === humanColor;
}

// ── Board (2D) ─────────────────────────────────────────────────────

function squareName(rank, file) { return FILES[file] + (rank + 1); }
export function sqEl(name) { return boardEl.querySelector(`.sq[data-sq="${name}"]`); }

export function findKingSquare(chess, color) {
    const board = chess.board();
    for (let r = 0; r < 8; r++) for (let f = 0; f < 8; f++) {
        const p = board[r][f];
        if (p && p.type === 'k' && p.color === color) {
            return FILES[f] + (8 - r);
        }
    }
    return null;
}

export function boardForView() {
    if (state.viewIdx === -1) return state.game;
    const tmp = new Chess();
    const moves = state.game.history({ verbose: true });
    for (let i = 0; i <= state.viewIdx && i < moves.length; i++) {
        tmp.move({ from: moves[i].from, to: moves[i].to, promotion: moves[i].promotion });
    }
    return tmp;
}

export function buildBoard() {
    boardEl.innerHTML = '';
    for (let displayRow = 0; displayRow < 8; displayRow++) {
        for (let displayCol = 0; displayCol < 8; displayCol++) {
            const rank = state.flipped ? displayRow : 7 - displayRow;
            const file = state.flipped ? 7 - displayCol : displayCol;
            const sq = document.createElement('div');
            sq.className = 'sq ' + (((rank + file) % 2 === 0) ? 'dark' : 'light');
            sq.dataset.sq = squareName(rank, file);
            boardEl.appendChild(sq);
        }
    }
    const fileLabels = state.flipped ? [...FILES].reverse() : FILES;
    const rankLabels = state.flipped ? [1,2,3,4,5,6,7,8] : [8,7,6,5,4,3,2,1];
    filesTop.innerHTML = filesBottom.innerHTML =
        fileLabels.map(f => `<div>${f}</div>`).join('');
    ranksLeft.innerHTML = ranksRight.innerHTML =
        rankLabels.map(r => `<div>${r}</div>`).join('');
}

export function renderPosition(opts = {}) {
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
            div.draggable = canDragLive() && piece.color === sideToMoveLive() && state.viewIdx === -1;
            div.dataset.sq = sq.dataset.sq;
            sq.appendChild(div);
        }
        const dot = document.createElement('div');
        dot.className = 'legal-dot';
        dot.style.display = 'none';
        sq.appendChild(dot);
    });

    if (state.lastMove && state.viewIdx === -1) {
        const a = sqEl(state.lastMove.from), b = sqEl(state.lastMove.to);
        if (a) a.classList.add('from');
        if (b) b.classList.add('to');
    }

    if (display.inCheck()) {
        const turn = display.turn();
        const kingSq = findKingSquare(display, turn);
        if (kingSq) sqEl(kingSq).classList.add('check');
    }

    if (state.selected) {
        const s = sqEl(state.selected);
        if (s) s.classList.add('selected');
        for (const m of state.legalForSelected) {
            const d = sqEl(m.to);
            if (d) {
                const dot = d.querySelector('.legal-dot');
                if (dot) dot.style.display = 'block';
            }
        }
    }

    fenEl.value = state.game.fen();
    syncBoard3D(opts.animatedMove);
}

export function syncBoard3D(animatedMove = null) {
    if (!state.board3d || !state.view3D) return;
    const display = boardForView();
    let animated = false;
    if (animatedMove && state.viewIdx === -1) {
        animated = state.board3d.animateMove(animatedMove);
    }
    if (!animated) {
        state.board3d.setBoardFromFen(display.fen());
    }
    let checkSq = null;
    if (display.inCheck()) checkSq = findKingSquare(display, display.turn());
    state.board3d.setHighlights({
        from: state.viewIdx === -1 ? state.lastMove?.from : null,
        to:   state.viewIdx === -1 ? state.lastMove?.to   : null,
        selected: state.viewIdx === -1 ? state.selected : null,
        legalSquares: state.viewIdx === -1 ? state.legalForSelected.map(m => m.to) : [],
        checkSquare: checkSq,
    });
}

// ── History list ───────────────────────────────────────────────────

export function renderHistory() {
    histEl.innerHTML = '';
    for (let i = 0; i < state.historySAN.length; i += 2) {
        const num = (i / 2) + 1;
        const li = document.createElement('li');
        li.dataset.idx = i;
        li.innerHTML =
            `<span style="color:#888">${num}.</span>` +
            `<span class="san" data-ply="${i}">${state.historySAN[i] || ''}</span>` +
            (state.historySAN[i+1] ? `<span class="san" data-ply="${i+1}">${state.historySAN[i+1]}</span>` : '');
        histEl.appendChild(li);
    }
    const cur = histEl.querySelector(
        `[data-ply="${state.viewIdx === -1 ? state.historySAN.length - 1 : state.viewIdx}"]`);
    if (cur) cur.parentElement.classList.add('current');
}

histEl.addEventListener('click', (ev) => {
    const span = ev.target.closest('[data-ply]');
    if (!span) return;
    const ply = parseInt(span.dataset.ply, 10);
    state.viewIdx = (ply === state.historySAN.length - 1) ? -1 : ply;
    renderPosition();
    renderHistory();
});

// ── Status / engine info ───────────────────────────────────────────

export function setStatus(text) { statusEl.textContent = text; }

export function updateStatus() {
    if (!state.engineReady) { setStatus('Đang kết nối máy…'); return; }
    if (state.gameOver) return;
    if (state.game.isCheckmate()) {
        const winner = state.game.turn() === 'w' ? 'Đen' : 'Trắng';
        setStatus(`Chiếu hết. ${winner} thắng.`);
        return;
    }
    if (state.game.isStalemate())            { setStatus('Hết nước (stalemate). Hòa.'); return; }
    if (state.game.isInsufficientMaterial()) { setStatus('Hòa (không đủ quân).');       return; }
    if (state.game.isThreefoldRepetition())  { setStatus('Hòa (lặp ba lần).');          return; }
    if (state.game.isDraw())                 { setStatus('Hòa (luật 50 nước).');        return; }
    const turn = state.game.turn() === 'w' ? 'Trắng' : 'Đen';
    if (state.waitingBestmove && state.pendingHint) setStatus(`Đang tính gợi ý…`);
    else if (state.waitingBestmove)                 setStatus(`Máy đang suy nghĩ (${turn})…`);
    else if (state.game.inCheck())                  setStatus(`${turn} bị chiếu — đến lượt ${turn}.`);
    else                                            setStatus(`Lượt: ${turn}`);
}

export function clearInfo() {
    infoDepth.textContent = '–';
    infoScore.textContent = '–';
    infoNodes.textContent = '–';
    infoNps.textContent   = '–';
    infoPv.textContent    = '–';
}

export function renderInfo(msg) {
    if (msg.depth != null) infoDepth.textContent = msg.depth;
    if (msg.nodes != null) infoNodes.textContent = msg.nodes.toLocaleString();
    if (msg.nps != null)   infoNps.textContent   = msg.nps.toLocaleString();
    if (msg.score) {
        let perspective = msg.score.value;
        if (state.game.turn() === 'b') perspective = -perspective;
        if (msg.score.kind === 'cp') {
            const sign = perspective >= 0 ? '+' : '';
            infoScore.textContent = `${sign}${(perspective / 100).toFixed(2)}`;
        } else if (msg.score.kind === 'mate') {
            infoScore.textContent = `M${msg.score.value}`;
        }
    }
    if (msg.pv) {
        try {
            const tmp = new Chess(state.game.fen());
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

// ── Player bars ────────────────────────────────────────────────────

export function renderPlayers() {
    let names, icons;
    if (state.mode === 'pve') {
        const human = humanSideEl.value === 'white' ? 'w' : 'b';
        const engine = human === 'w' ? 'b' : 'w';
        names = {}; icons = {};
        names[human]  = 'Bạn';
        names[engine] = state.currentPersona ? state.currentPersona.name : 'Máy';
        icons[human]  = '👤';
        icons[engine] = state.currentPersona ? state.currentPersona.emoji : '🤖';
    } else if (state.mode === 'eve') {
        names = { w: 'Máy Trắng', b: 'Máy Đen' };
        icons = { w: '🤖', b: '🤖' };
    } else { // pvp
        names = { w: 'Trắng', b: 'Đen' };
        icons = { w: '👤', b: '👤' };
    }
    const top = topPlayerColor();
    const bot = top === 'w' ? 'b' : 'w';
    playerTopName.textContent    = names[top];
    playerBottomName.textContent = names[bot];
    playerTopIcon.textContent    = icons[top];
    playerBottomIcon.textContent = icons[bot];
}

// ── Promotion modal ────────────────────────────────────────────────

export function askPromotion() {
    const turn = state.game.turn();
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
