// Chess clock system (incremental Fischer time).
// Hooks: setOnTimeOutHandler(fn) lets app.js handle the end-of-game flow.
import { state } from './state.js';

const timeControlEl = document.getElementById('time-control');
const clockTopEl    = document.getElementById('clock-top');
const clockBottomEl = document.getElementById('clock-bottom');

let onTimeOutHandler = () => {};
export function setOnTimeOutHandler(fn) { onTimeOutHandler = fn; }

function topColor() { return state.flipped ? 'w' : 'b'; }

export function setupClockFromSelect() {
    const tcStr = timeControlEl.value;
    if (tcStr === 'none') {
        state.timeControl = null;
        state.clockMs = { w: 0, b: 0 };
        stopClock();
        renderClocks();
        return;
    }
    const [secs, inc] = tcStr.split('+').map(Number);
    state.timeControl = { initial: secs * 1000, increment: inc * 1000 };
    state.clockMs = { w: state.timeControl.initial, b: state.timeControl.initial };
    stopClock();
    renderClocks();
}

export function formatMs(ms) {
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

export function renderClocks() {
    if (!state.timeControl) {
        clockTopEl.textContent = '';
        clockBottomEl.textContent = '';
        clockTopEl.className = 'clock';
        clockBottomEl.className = 'clock';
        return;
    }
    const top = topColor();
    const bot = top === 'w' ? 'b' : 'w';
    clockTopEl.textContent    = formatMs(state.clockMs[top]);
    clockBottomEl.textContent = formatMs(state.clockMs[bot]);
    const stm = state.game.turn();
    setClockClass(clockTopEl,    top, stm);
    setClockClass(clockBottomEl, bot, stm);
}

function setClockClass(el, color, stm) {
    el.classList.remove('active', 'warn', 'dead');
    if (state.clockMs[color] <= 0) { el.classList.add('dead'); return; }
    if (state.clockRunning && stm === color && !state.gameOver) el.classList.add('active');
    if (state.clockMs[color] < 30000) el.classList.add('warn');
}

export function startClock() {
    if (!state.timeControl || state.gameOver) return;
    if (state.game.isGameOver()) return;
    if (state.clockRunning) return;
    state.clockRunning = true;
    state.lastTickTs = performance.now();
    if (state.clockTickHandle) clearInterval(state.clockTickHandle);
    state.clockTickHandle = setInterval(tickClock, 100);
    renderClocks();
}
export function stopClock() {
    state.clockRunning = false;
    if (state.clockTickHandle) {
        clearInterval(state.clockTickHandle);
        state.clockTickHandle = null;
    }
    renderClocks();
}
function tickClock() {
    if (!state.clockRunning || !state.timeControl) return;
    const now = performance.now();
    const delta = now - state.lastTickTs;
    state.lastTickTs = now;
    const stm = state.game.turn();
    state.clockMs[stm] -= delta;
    if (state.clockMs[stm] <= 0) {
        state.clockMs[stm] = 0;
        stopClock();
        onTimeOutHandler(stm);
        return;
    }
    renderClocks();
}
export function applyIncrement(colorThatJustMoved) {
    if (!state.timeControl) return;
    if (state.clockMs[colorThatJustMoved] > 0) {
        state.clockMs[colorThatJustMoved] += state.timeControl.increment;
    }
}

timeControlEl.addEventListener('change', () => {
    setupClockFromSelect();
    if (!state.gameOver && state.historySAN.length > 0 && state.timeControl) {
        startClock();
    }
});
