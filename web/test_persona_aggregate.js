// Aggregate judge reports across multiple personas → SUMMARY.md
//
// Usage:
//   node test_persona_aggregate.js <log-dir> [--out=SUMMARY.md]

const fs = require('fs');
const path = require('path');

const LOG_DIR = process.argv[2];
if (!LOG_DIR || !fs.existsSync(LOG_DIR)) {
    console.error('Usage: node test_persona_aggregate.js <log-dir> [--out=...]');
    process.exit(1);
}
const argv = Object.fromEntries(process.argv.slice(3)
    .map(a => a.replace(/^--/, '').split('=')).map(([k, v]) => [k, v ?? true]));
const OUT = argv.out || path.join(LOG_DIR, 'SUMMARY.md');

const judgeFiles = fs.readdirSync(LOG_DIR).filter(f => f.endsWith('-judge.json')).sort();
const rows = [];
const allWorst = [];

for (const f of judgeFiles) {
    const data = JSON.parse(fs.readFileSync(path.join(LOG_DIR, f), 'utf8'));
    const s = data.summary;
    const persona = s.persona;
    rows.push({
        emoji: persona.emoji || '',
        name: persona.name,
        elo: persona.elo,
        chats: s.total_chats,
        nat: s.avg_naturalness,
        char: s.avg_in_character,
        var_: s.avg_variety,
        cringe: `${s.cringe_flagged}/${s.total_chats}`,
        cringeRate: s.cringe_rate,
    });
    const worst = data.results
        .filter(r => r.judge?.naturalness)
        .map(r => ({ persona: persona.name, ply: r.ply, event: r.event, reaction: r.reaction, mood: r.mood?.level, score: r.judge.naturalness + r.judge.in_character + r.judge.variety, reply: r.reply, cringe: r.judge.cringe }))
        .sort((a, b) => a.score - b.score)
        .slice(0, 3);
    allWorst.push(...worst);
}

const lines = [];
lines.push('# Persona test SUMMARY\n');
lines.push(`Generated: ${new Date().toISOString()}\n`);
lines.push(`Log dir: \`${LOG_DIR}\`\n`);
lines.push('## Bảng tổng (thang 1-5)\n');
lines.push('| Persona | Elo | Chats | Naturalness | In-character | Variety | Cringe |');
lines.push('|---|--:|--:|--:|--:|--:|--:|');
for (const r of rows) {
    const cringePct = (r.cringeRate * 100).toFixed(0) + '%';
    lines.push(`| ${r.emoji} ${r.name} | ${r.elo} | ${r.chats} | ${r.nat.toFixed(2)} | ${r.char.toFixed(2)} | ${r.var_.toFixed(2)} | ${r.cringe} (${cringePct}) |`);
}

const overall = {
    nat: rows.reduce((s, r) => s + r.nat, 0) / rows.length,
    char: rows.reduce((s, r) => s + r.char, 0) / rows.length,
    var_: rows.reduce((s, r) => s + r.var_, 0) / rows.length,
    cringe: rows.reduce((s, r) => s + r.cringeRate, 0) / rows.length,
};
lines.push('\n## Overall (avg across personas)\n');
lines.push(`- Naturalness: **${overall.nat.toFixed(2)} / 5**`);
lines.push(`- In-character: **${overall.char.toFixed(2)} / 5**`);
lines.push(`- Variety: **${overall.var_.toFixed(2)} / 5**`);
lines.push(`- Cringe rate: **${(overall.cringe * 100).toFixed(0)}%**`);

const sortedWorst = allWorst.sort((a, b) => a.score - b.score).slice(0, 15);
lines.push('\n## Top 15 câu kém nhất (cross-persona)\n');
for (const w of sortedWorst) {
    lines.push(`- **[${w.persona}]** (score ${w.score}/15, mood=${w.mood}, reaction=${w.reaction})`);
    lines.push(`  > ${w.reply}`);
    if (w.cringe) lines.push(`  ⚠ ${w.cringe}`);
}

fs.writeFileSync(OUT, lines.join('\n') + '\n');
console.log(`✓ Summary saved: ${OUT}`);
console.log(`\nOverall: nat=${overall.nat.toFixed(2)} char=${overall.char.toFixed(2)} var=${overall.var_.toFixed(2)} cringe=${(overall.cringe*100).toFixed(0)}%`);
