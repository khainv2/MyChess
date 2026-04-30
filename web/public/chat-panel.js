// Chat panel UI: rendering messages, typing indicator, open/close toggle,
// input form. Pure UI — no LLM logic. App registers a send-handler via
// `setSendChatHandler` so we don't import persona-chat (avoids circular dep).
import { state } from './state.js';

const chatLogEl       = document.getElementById('chat-log');
const chatFormEl      = document.getElementById('chat-form');
const chatInputEl     = document.getElementById('chat-input');
const chatSendEl      = document.getElementById('chat-send');
const chatStatusEl    = document.getElementById('chat-status');
const chatPersonaTagEl= document.getElementById('chat-persona-tag');
const btnChatClose    = document.getElementById('btn-chat-close');
const btnChatOpen     = document.getElementById('btn-chat-open');
const layoutEl        = document.querySelector('.layout');

let onUserChatSubmit = (_text) => {};
export function setSendChatHandler(fn) { onUserChatSubmit = fn; }

// ── Log messages ──

export function clearChatLog() {
    chatLogEl.innerHTML = '';
    state.chatTypingNode = null;
}

export function scrollChatToBottom() {
    chatLogEl.scrollTop = chatLogEl.scrollHeight;
}

export function appendChatMsg(kind, text, opts = {}) {
    // kind: 'persona' | 'user' | 'system'
    const wrap = document.createElement('div');
    wrap.className = 'chat-msg chat-msg-' + kind;

    if (kind !== 'system') {
        const head = document.createElement('div');
        head.className = 'chat-msg-head';
        const emoji = document.createElement('span');
        emoji.className = 'head-emoji';
        emoji.textContent = kind === 'persona'
            ? (opts.emoji || (state.currentPersona && state.currentPersona.emoji) || '🤖')
            : '👤';
        const name = document.createElement('span');
        name.textContent = kind === 'persona'
            ? (opts.name || (state.currentPersona && state.currentPersona.name) || 'Đối thủ')
            : 'Bạn';
        head.appendChild(emoji); head.appendChild(name);
        wrap.appendChild(head);
    }
    const body = document.createElement('div');
    body.className = 'chat-msg-body';
    body.textContent = text;
    wrap.appendChild(body);

    chatLogEl.appendChild(wrap);
    scrollChatToBottom();
    if (kind === 'persona') notifyUnreadIfClosed();
    return wrap;
}

// ── Typing indicator ──

export function showTypingIndicator() {
    if (state.chatTypingNode || !state.currentPersona) return;
    const wrap = document.createElement('div');
    wrap.className = 'chat-msg chat-msg-persona';
    const head = document.createElement('div');
    head.className = 'chat-msg-head';
    const emoji = document.createElement('span');
    emoji.className = 'head-emoji';
    emoji.textContent = state.currentPersona.emoji || '🤖';
    const name = document.createElement('span');
    name.textContent = state.currentPersona.name;
    head.appendChild(emoji); head.appendChild(name);
    wrap.appendChild(head);
    const dots = document.createElement('div');
    dots.className = 'chat-msg-typing';
    dots.innerHTML = '<span></span><span></span><span></span>';
    wrap.appendChild(dots);
    chatLogEl.appendChild(wrap);
    state.chatTypingNode = wrap;
    scrollChatToBottom();
}
export function hideTypingIndicator() {
    if (state.chatTypingNode) {
        state.chatTypingNode.remove();
        state.chatTypingNode = null;
    }
}

// ── Input state / status line ──

export function setChatStatus(text) {
    if (chatStatusEl) chatStatusEl.textContent = text || '';
}

export function refreshChatInputState() {
    const enabled = !!state.currentPersona && state.mode === 'pve' && !state.chatBusy && state.engineReady;
    chatInputEl.disabled = !enabled;
    chatSendEl.disabled  = !enabled || !chatInputEl.value.trim();
    if (!state.currentPersona) {
        chatInputEl.placeholder = 'Chọn nhân vật ở mục Trận đấu để chat…';
    } else if (state.mode !== 'pve') {
        chatInputEl.placeholder = 'Chat chỉ khả dụng ở chế độ Người vs Máy.';
    } else {
        chatInputEl.placeholder = 'Nhắn cho đối thủ… (Enter gửi, Shift+Enter xuống dòng)';
    }
    if (chatPersonaTagEl) {
        chatPersonaTagEl.textContent = state.currentPersona
            ? `· ${state.currentPersona.emoji} ${state.currentPersona.name}`
            : '';
    }
}

// ── Panel open/close ──

export function applyChatPanelState() {
    if (state.chatPanelOpen) {
        layoutEl.classList.remove('chat-collapsed');
        btnChatOpen.classList.remove('visible', 'has-unread');
        scrollChatToBottom();
    } else {
        layoutEl.classList.add('chat-collapsed');
        btnChatOpen.classList.add('visible');
    }
}

export function openChatPanel() {
    state.chatPanelOpen = true;
    localStorage.setItem('mychess.chatPanel', 'open');
    applyChatPanelState();
    refreshChatInputState();
}
export function closeChatPanel() {
    state.chatPanelOpen = false;
    localStorage.setItem('mychess.chatPanel', 'closed');
    applyChatPanelState();
}
function notifyUnreadIfClosed() {
    if (!state.chatPanelOpen) btnChatOpen.classList.add('has-unread');
}

// ── Listeners ──

btnChatClose.addEventListener('click', closeChatPanel);
btnChatOpen.addEventListener('click', openChatPanel);

chatFormEl.addEventListener('submit', (ev) => {
    ev.preventDefault();
    const v = chatInputEl.value;
    chatInputEl.value = '';
    refreshChatInputState();
    onUserChatSubmit(v);
});
chatInputEl.addEventListener('input', refreshChatInputState);
chatInputEl.addEventListener('keydown', (ev) => {
    if (ev.key === 'Enter' && !ev.shiftKey && !ev.isComposing) {
        ev.preventDefault();
        chatFormEl.requestSubmit();
    }
});
