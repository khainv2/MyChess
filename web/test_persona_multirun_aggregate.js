// Aggregate N run × 7 persona thành báo cáo cuối có variance/median/min/max.
//
// Usage: node test_persona_multirun_aggregate.js <root-dir> --runs=N --out=...

const fs = require('fs');
const path = require('path');

const ROOT = process.argv[2];
if (!ROOT) { console.error('Usage: node test_persona_multirun_aggregate.js <root-dir> [--runs=N] [--out=...]'); process.exit(1); }
const argv = Object.fromEntries(process.argv.slice(3).map(a => a.replace(/^--/, '').split('=')).map(([k, v]) => [k, v ?? true]));
const N = parseInt(argv.runs || '3', 10);
const OUT = argv.out || path.join(ROOT, 'MULTIRUN.md');

// Load tất cả judge files từ runs.
const allRuns = [];   // [{ runIdx, persona, chats, nat, char, var, cringeRate, cringes:[{score, reply, cringe, mood, reaction}] }]
for (let i = 1; i <= N; i++) {
    const dir = path.join(ROOT, `run-${i}`);
    if (!fs.existsSync(dir)) { console.warn(`Skip run-${i}: not exists`); continue; }
    const files = fs.readdirSync(dir).filter(f => f.endsWith('-judge.json'));
    for (const f of files) {
        const data = JSON.parse(fs.readFileSync(path.join(dir, f), 'utf8'));
        const s = data.summary;
        if (!s.persona) continue;
        const cringes = data.results
            .filter(r => r.judge && r.judge.cringe)
            .map(r => ({ score: (r.judge.naturalness||0)+(r.judge.in_character||0)+(r.judge.variety||0), reply: r.reply, cringe: r.judge.cringe, mood: r.mood?.level, reaction: r.reaction }));
        allRuns.push({
            runIdx: i,
            personaId: s.persona.id,
            personaName: s.persona.name,
            elo: s.persona.elo,
            chats: s.total_chats,
            nat: s.avg_naturalness,
            char: s.avg_in_character,
            varScore: s.avg_variety,
            cringeRate: s.cringe_rate,
            cringeFlagged: s.cringe_flagged,
            cringes,
        });
    }
}

// Group by persona.
const byPersona = new Map();
for (const r of allRuns) {
    if (!byPersona.has(r.personaId)) byPersona.set(r.personaId, []);
    byPersona.get(r.personaId).push(r);
}

function median(arr) {
    if (!arr.length) return 0;
    const s = [...arr].sort((a, b) => a - b);
    const m = Math.floor(s.length / 2);
    return s.length % 2 ? s[m] : (s[m - 1] + s[m]) / 2;
}
function mean(arr)  { return arr.length ? arr.reduce((a, b) => a + b, 0) / arr.length : 0; }
function stdev(arr) { if (arr.length < 2) return 0; const m = mean(arr); return Math.sqrt(arr.reduce((s, x) => s + (x - m) ** 2, 0) / (arr.length - 1)); }

const personaSummary = [];
for (const [pid, runs] of byPersona) {
    const cringeRates = runs.map(r => r.cringeRate);
    const nats        = runs.map(r => r.nat);
    const chars       = runs.map(r => r.char);
    const vars        = runs.map(r => r.varScore);
    const totalChats  = runs.reduce((s, r) => s + r.chats, 0);
    const totalCringe = runs.reduce((s, r) => s + r.cringeFlagged, 0);
    personaSummary.push({
        id: pid,
        name: runs[0].personaName,
        elo: runs[0].elo,
        nRuns: runs.length,
        avgChats: mean(runs.map(r => r.chats)),
        totalChats, totalCringe,
        cringeMean: mean(cringeRates),
        cringeMedian: median(cringeRates),
        cringeMin: Math.min(...cringeRates),
        cringeMax: Math.max(...cringeRates),
        cringeStd: stdev(cringeRates),
        natMean:  mean(nats),
        charMean: mean(chars),
        varMean:  mean(vars),
        // Pooled rate: tổng cringe / tổng chats — trung bình có trọng số theo sample.
        cringePooled: totalChats ? totalCringe / totalChats : 0,
        allCringes: runs.flatMap(r => r.cringes),
    });
}

// Overall (avg across persona qua N run).
const overallCringePooled = (() => {
    const tc = personaSummary.reduce((s, p) => s + p.totalChats, 0);
    const tcr = personaSummary.reduce((s, p) => s + p.totalCringe, 0);
    return tc ? tcr / tc : 0;
})();
const overallNat  = mean(personaSummary.map(p => p.natMean));
const overallChar = mean(personaSummary.map(p => p.charMean));
const overallVar  = mean(personaSummary.map(p => p.varMean));
const overallCringeMean = mean(personaSummary.map(p => p.cringeMean));
const overallCringeStd  = stdev(personaSummary.map(p => p.cringeMean));

// Per-run overall cringe (tổng cringe trên tổng chats trong run đó)
const runOverall = [];
for (let i = 1; i <= N; i++) {
    const runRows = allRuns.filter(r => r.runIdx === i);
    if (!runRows.length) continue;
    const tc  = runRows.reduce((s, r) => s + r.chats, 0);
    const tcr = runRows.reduce((s, r) => s + r.cringeFlagged, 0);
    runOverall.push({ run: i, chats: tc, cringe: tcr, rate: tc ? tcr / tc : 0,
        nat: mean(runRows.map(r => r.nat)),
        char: mean(runRows.map(r => r.char)),
        var_: mean(runRows.map(r => r.varScore)),
    });
}

const lines = [];
lines.push('# Persona MULTI-RUN report');
lines.push('');
lines.push(`Generated: ${new Date().toISOString()}`);
lines.push(`Root dir: \`${ROOT}\``);
lines.push(`Runs: ${N}`);
lines.push('');
lines.push('## Per-run overall (variance giữa các run)');
lines.push('');
lines.push('| Run | Chats | Cringe | Rate | Naturalness | In-char | Variety |');
lines.push('|---|--:|--:|--:|--:|--:|--:|');
for (const r of runOverall) {
    lines.push(`| #${r.run} | ${r.chats} | ${r.cringe} | ${(r.rate*100).toFixed(0)}% | ${r.nat.toFixed(2)} | ${r.char.toFixed(2)} | ${r.var_.toFixed(2)} |`);
}
const rateValues = runOverall.map(r => r.rate);
lines.push(`| **mean ± std** | — | — | **${(mean(rateValues)*100).toFixed(0)}% ± ${(stdev(rateValues)*100).toFixed(0)}%** | ${mean(runOverall.map(r => r.nat)).toFixed(2)} | ${mean(runOverall.map(r => r.char)).toFixed(2)} | ${mean(runOverall.map(r => r.var_)).toFixed(2)} |`);
lines.push('');
lines.push('## Per-persona (median/min/max cringe rate qua N run)');
lines.push('');
lines.push('| Persona | Elo | Runs | Tổng chats | Cringe pooled | Median | Min..Max | ±Std | Nat | Char | Var |');
lines.push('|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|');
personaSummary.sort((a, b) => b.cringePooled - a.cringePooled);
for (const p of personaSummary) {
    const range = `${(p.cringeMin*100).toFixed(0)}..${(p.cringeMax*100).toFixed(0)}%`;
    lines.push(`| ${p.name} | ${p.elo} | ${p.nRuns} | ${p.totalChats} | **${(p.cringePooled*100).toFixed(0)}%** | ${(p.cringeMedian*100).toFixed(0)}% | ${range} | ${(p.cringeStd*100).toFixed(0)}% | ${p.natMean.toFixed(2)} | ${p.charMean.toFixed(2)} | ${p.varMean.toFixed(2)} |`);
}
lines.push('');
lines.push('## Overall');
lines.push('');
lines.push(`- **Cringe rate (pooled)**: ${(overallCringePooled*100).toFixed(1)}%   ← weighted theo sample size`);
lines.push(`- **Cringe rate (mean of persona means)**: ${(overallCringeMean*100).toFixed(1)}% ± ${(overallCringeStd*100).toFixed(1)}%`);
lines.push(`- Naturalness: ${overallNat.toFixed(2)} / 5`);
lines.push(`- In-character: ${overallChar.toFixed(2)} / 5`);
lines.push(`- Variety: ${overallVar.toFixed(2)} / 5`);
lines.push('');
lines.push('## Top cringe (cross-persona, cross-run, sort theo score thấp nhất)');
lines.push('');
const allCringes = personaSummary.flatMap(p => p.allCringes.map(c => ({ ...c, persona: p.name })));
allCringes.sort((a, b) => a.score - b.score);
for (const c of allCringes.slice(0, 20)) {
    lines.push(`- **[${c.persona}]** (score ${c.score}/15, mood=${c.mood}, reaction=${c.reaction})`);
    lines.push(`  > ${c.reply}`);
    if (c.cringe) lines.push(`  ⚠ ${c.cringe}`);
}
lines.push('');

fs.writeFileSync(OUT, lines.join('\n'));
console.log(`✓ Multi-run report saved: ${OUT}`);
console.log(`\n══ Multi-run summary ══`);
console.log(`Per-run cringe rate: ${rateValues.map(v => (v*100).toFixed(0) + '%').join(' / ')}`);
console.log(`Mean ± std: ${(mean(rateValues)*100).toFixed(1)}% ± ${(stdev(rateValues)*100).toFixed(1)}%`);
console.log(`Pooled (weighted by sample): ${(overallCringePooled*100).toFixed(1)}%`);
