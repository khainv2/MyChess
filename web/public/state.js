// Shared mutable state + small helpers used by every module.
// Other files import { state, send, ... } and read/write state.x directly.
// Live binding via property access keeps this safe across modules.

import { Chess } from './vendor/chess.js';

export const FILES = ['a','b','c','d','e','f','g','h'];
export const MAX_RECONNECT = 5;
export const MAX_CHAT_TURNS = 60;
export const EVENT_TAG = '[SỰ-KIỆN-BÀN-CỜ]';

export const state = {
    // ── Core game ──
    game: new Chess(),
    flipped: false,
    selected: null,
    legalForSelected: [],
    lastMove: null,
    historySAN: [],
    viewIdx: -1,
    engineReady: false,
    waitingBestmove: false,
    pendingHint: false,
    mode: 'pve',
    gameOver: false,
    plyCount: 0,

    // ── 3D board ──
    board3d: null,
    view3D: localStorage.getItem('mychess.view3d') !== 'off',

    // ── Sound ──
    audioCtx: null,
    soundOn: localStorage.getItem('mychess.sound') !== 'off',

    // ── Clock ──
    timeControl: null,
    clockMs: { w: 0, b: 0 },
    clockRunning: false,
    clockTickHandle: null,
    lastTickTs: 0,

    // ── Persona / chat ──
    personas: [],
    currentPersona: null,
    chatHistory: [],
    chatBusy: false,
    chatTypingNode: null,
    lastChatPly: -10,
    chatPanelOpen: localStorage.getItem('mychess.chatPanel') !== 'closed',
    // Lịch sử reaction-type gần đây để chống lặp pattern (Phase 1)
    recentReactions: [],
    // 3 reply gần nhất của bot — feed lại vào prompt để LLM tránh lặp phrase
    recentReplies: [],

    // ── Persona mood (Phase 2) ──
    personaMood: {
        level: 'neutral',
        intensity: 0,
        cause: null,
        sinceMove: 0,
        prevEvalCp: null,
        stableSince: 0,
        lastUpdated: 0,
    },
    personaDailyMood: null,  // string text, bốc 1 lần / session

    // ── Precompute (Phase 3) ──
    preComputed: {
        forFen: null,
        uci: null,
        san: null,
        scoreCp: null,
        computedAt: 0,
        requestId: null,
    },
    idleTimerHandle: null,        // setTimeout id chờ user idle (precognition)
    fenBeforeUserMove: null,      // lưu để compare hậu kiểm
    lastMoveQuality: null,        // 'matched'|'differed'|null — anti-repeat

    // ── Feature flags (per-phase enhancement, persist localStorage) ──
    featureFlags: {
        theatrical:  localStorage.getItem('mychess.ff.theatrical') !== 'off',
        mood:        localStorage.getItem('mychess.ff.mood')       !== 'off',
        precompute:  localStorage.getItem('mychess.ff.precompute') !== 'off',
        memory:      localStorage.getItem('mychess.ff.memory')     !== 'off',
        idleTrigger: localStorage.getItem('mychess.ff.idleTrigger')!== 'off',
    },

    // ── WebSocket ──
    ws: null,
    reconnectAttempts: 0,
    reconnectTimer: null,
    resumingSession: false,
};

export function send(obj) {
    if (state.ws && state.ws.readyState === WebSocket.OPEN) {
        state.ws.send(JSON.stringify(obj));
    }
}
