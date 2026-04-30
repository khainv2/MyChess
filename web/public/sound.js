// WebAudio sound effects + 🔔 toggle button.
import { state } from './state.js';

const btnSound = document.getElementById('btn-sound');

export function syncSoundButton() {
    btnSound.textContent = state.soundOn ? '🔔' : '🔕';
    btnSound.classList.toggle('muted', !state.soundOn);
}

function ensureAudio() {
    if (!state.soundOn) return null;
    try {
        if (!state.audioCtx) state.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        if (state.audioCtx.state === 'suspended') state.audioCtx.resume();
        return state.audioCtx;
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
    osc.connect(gain); gain.connect(ctx.destination);
    osc.start(t0);
    osc.stop(t0 + duration / 1000 + 0.01);
}

export function playMoveSound(result) {
    if (!result) return;
    if (state.game.inCheck()) { playCheckSound(); return; }
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
export function playCheckSound() {
    tone(880, 100, 'square', 0.10);
    tone(660, 130, 'square', 0.08, 110);
}
export function playMateSound() {
    tone(220, 200, 'sawtooth', 0.12);
    tone(180, 280, 'sawtooth', 0.12, 200);
}
export function playStartSound() {
    tone(523, 70, 'sine', 0.08);
    tone(659, 70, 'sine', 0.08, 70);
    tone(784, 110, 'sine', 0.08, 140);
}

btnSound.addEventListener('click', () => {
    state.soundOn = !state.soundOn;
    localStorage.setItem('mychess.sound', state.soundOn ? 'on' : 'off');
    syncSoundButton();
    if (state.soundOn) tone(660, 60); // confirm
});
syncSoundButton();
