// Feature flag UI binding — render danh sách toggle, persist localStorage.
// Phase 1 đã ship: theatrical. Các phase sau placeholder, hiển thị disabled.
import { state } from './state.js';

const FLAGS = [
    { key: 'theatrical',  label: 'Đa dạng phản ứng',     desc: 'Bốc ngẫu nhiên thể loại bình luận / off-topic / im lặng',         shipped: true  },
    { key: 'mood',        label: 'Cảm xúc theo thế cờ',  desc: 'Bot có tâm trạng (panic/gloating/bored…) thay đổi theo eval engine', shipped: true  },
    { key: 'precompute',  label: 'Engine tiên tri',      desc: 'Persona ≤1500 elo: bot dự đoán nước bạn & nhận xét đúng/sai (chỉ ám chỉ, không lộ best move)', shipped: true  },
    { key: 'memory',      label: 'Nhớ ván trước',        desc: 'Bot nhớ kết quả ván trước với bạn (Phase 4 — chưa ship)',         shipped: false },
    { key: 'idleTrigger', label: 'Tự nói khi bạn idle',  desc: 'Bot lên tiếng nếu bạn nghĩ quá lâu (Phase 5 — chưa ship)',        shipped: false },
];

export function initFeatureFlagsUI(containerId) {
    const root = document.getElementById(containerId);
    if (!root) return;
    root.innerHTML = '';
    for (const f of FLAGS) {
        const wrap = document.createElement('label');
        wrap.className = 'ff-row' + (f.shipped ? '' : ' ff-row-disabled');
        const cb = document.createElement('input');
        cb.type = 'checkbox';
        cb.id = `ff-${f.key}`;
        cb.checked = !!state.featureFlags[f.key];
        if (!f.shipped) {
            cb.disabled = true;
            cb.checked = false;
        }
        cb.addEventListener('change', () => {
            state.featureFlags[f.key] = cb.checked;
            localStorage.setItem(`mychess.ff.${f.key}`, cb.checked ? 'on' : 'off');
        });
        const meta = document.createElement('span');
        meta.className = 'ff-meta';
        const title = document.createElement('span');
        title.className = 'ff-title';
        title.textContent = f.label;
        const desc = document.createElement('span');
        desc.className = 'ff-desc';
        desc.textContent = f.desc;
        meta.appendChild(title);
        meta.appendChild(desc);
        wrap.appendChild(cb);
        wrap.appendChild(meta);
        root.appendChild(wrap);
    }
}
