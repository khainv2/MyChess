// LLM-as-judge cho log của test_persona_pipeline.js.
// Đọc log JSON, gửi từng câu reply lên cùng vLLM kèm context để chấm:
//   - naturalness (1-5): câu nghe có giống người thật không
//   - in_character (1-5): có đúng giọng/tính cách persona không
//   - repetition (1-5): có lặp pattern với câu trước không
//   - cringe_flags: list các điểm cringe cụ thể
//
// Usage:
//   node test_persona_judge.js <log.json> [--out=report.json]

const path = require('path');
const fs = require('fs');

const LOG_PATH = process.argv[2];
if (!LOG_PATH || !fs.existsSync(LOG_PATH)) {
    console.error('Usage: node test_persona_judge.js <log.json> [--out=report.json]');
    process.exit(1);
}
const argv = Object.fromEntries(process.argv.slice(3)
    .map(a => a.replace(/^--/, '').split('=')).map(([k, v]) => [k, v ?? true]));
const OUT_PATH = argv.out || LOG_PATH.replace(/\.json$/, '-judge.json');

const LLM_URL   = process.env.LLM_BASE_URL || 'http://localhost:3073';
const LLM_MODEL = process.env.LLM_MODEL    || 'qwen3.6-35b-a3b';

const data = JSON.parse(fs.readFileSync(LOG_PATH, 'utf8'));
const turns = data.log.filter(x => x.reply && !x.reply.startsWith('('));
console.log(`▶ Persona: ${data.persona.name} (elo ${data.persona.elo})`);
console.log(`▶ Total chat replies: ${turns.length}\n`);

async function judge(turn, idx, prevReplies) {
    const sys = `Bạn là chuyên gia đánh giá chatbot. Bạn sẽ chấm 1 câu trả lời của bot (đóng vai nhân vật ${data.persona.name}: ${data.persona.summary}).

Đánh giá theo tiêu chí, thang 1-5 (1=rất kém, 5=rất tốt):

1. naturalness: câu nghe có giống người thật nói không (5 = tự nhiên như người, 1 = rõ ràng máy tạo / robot / cứng đơ)
2. in_character: có đúng tính cách + giọng + dialect persona không (5 = rất đúng, 1 = sai hoàn toàn)
3. variety: có khác biệt với các câu trước không, có lặp pattern không (5 = mới mẻ, 1 = lặp y hệt cấu trúc câu trước)

Trả về CHÍNH XÁC định dạng JSON một dòng:
{"naturalness": N, "in_character": N, "variety": N, "cringe": "mô tả ngắn 1 câu nếu có điểm cringe, hoặc null"}`;

    const usr = `Persona system_prompt: ${data.persona.summary || data.persona.name}

Sự kiện vừa xảy ra: event=${turn.event}, reaction=${turn.reaction}${turn.topic ? ', topic="' + turn.topic + '"' : ''}, mood=${turn.mood?.level || 'neutral'}.

${prevReplies.length ? 'Các câu trước đó của bot:\n' + prevReplies.map((r, i) => `${i+1}. ${r}`).join('\n') + '\n' : ''}
Câu mới của bot CẦN CHẤM:
"${turn.reply}"

Trả về JSON:`;

    const res = await fetch(`${LLM_URL}/v1/chat/completions`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            model: LLM_MODEL,
            messages: [{ role: 'system', content: sys }, { role: 'user', content: usr }],
            max_tokens: 150,
            temperature: 0.3,
            chat_template_kwargs: { enable_thinking: false },
        }),
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const j = await res.json();
    const text = (j.choices?.[0]?.message?.content || '').trim();
    // Try to extract JSON
    const m = text.match(/\{[^}]*\}/);
    if (!m) return { raw: text, parse_error: true };
    try { return JSON.parse(m[0]); }
    catch (e) { return { raw: text, parse_error: true }; }
}

(async () => {
    const results = [];
    const prevReplies = [];
    for (let i = 0; i < turns.length; i++) {
        const t = turns[i];
        process.stdout.write(`  [${i+1}/${turns.length}] event=${t.event.padEnd(12)} reaction=${(t.reaction||'').padEnd(20)} `);
        let scored;
        try {
            scored = await judge(t, i, prevReplies.slice(-3));  // chỉ truyền 3 câu trước để judge
        } catch (e) {
            scored = { error: e.message };
        }
        const n = scored.naturalness || 0, c = scored.in_character || 0, v = scored.variety || 0;
        process.stdout.write(`nat=${n} char=${c} var=${v}` + (scored.cringe ? ` ⚠ ${scored.cringe.slice(0, 60)}` : '') + '\n');
        results.push({ ...t, judge: scored });
        prevReplies.push(t.reply);
    }

    // Aggregate
    const valid = results.filter(r => r.judge?.naturalness);
    const avg = (k) => valid.length ? +(valid.reduce((s, r) => s + (r.judge[k] || 0), 0) / valid.length).toFixed(2) : 0;
    const flagged = results.filter(r => r.judge?.cringe).length;
    const summary = {
        persona: data.persona,
        total_chats: turns.length,
        avg_naturalness: avg('naturalness'),
        avg_in_character: avg('in_character'),
        avg_variety: avg('variety'),
        cringe_flagged: flagged,
        cringe_rate: turns.length ? +(flagged / turns.length).toFixed(2) : 0,
    };

    fs.writeFileSync(OUT_PATH, JSON.stringify({ summary, results }, null, 2));
    console.log('\n══ Summary ══');
    console.log(`  Naturalness:   ${summary.avg_naturalness} / 5`);
    console.log(`  In-character:  ${summary.avg_in_character} / 5`);
    console.log(`  Variety:       ${summary.avg_variety} / 5`);
    console.log(`  Cringe flags:  ${flagged}/${turns.length} (${(summary.cringe_rate*100).toFixed(0)}%)`);
    console.log(`\n  Report saved: ${OUT_PATH}`);

    // List worst lines
    const worst = valid.sort((a,b) => (a.judge.naturalness + a.judge.in_character + a.judge.variety) - (b.judge.naturalness + b.judge.in_character + b.judge.variety)).slice(0, 5);
    if (worst.length) {
        console.log('\n  Top 5 câu kém nhất:');
        for (const w of worst) console.log(`    [${w.ply}] (n${w.judge.naturalness}/c${w.judge.in_character}/v${w.judge.variety}) ${w.reply}` + (w.judge.cringe ? `  ⚠ ${w.judge.cringe}` : ''));
    }
})().catch(e => { console.error('FATAL:', e); process.exit(1); });
