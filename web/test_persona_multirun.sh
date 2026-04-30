#!/usr/bin/env bash
# Multi-run wrapper: chạy test_persona_full.sh N lần để đo variance.
# Mỗi run vào dir riêng, sau đó aggregate qua test_persona_multirun_aggregate.js.
set -e
cd "$(dirname "$0")"

RUNS=${RUNS:-3}
TURNS=${TURNS:-30}
RUN_ID=$(date +%Y%m%d-%H%M%S)
ROOT=logs/multi-$RUN_ID
mkdir -p "$ROOT"

echo "▶ Multi-run ID: $RUN_ID  |  runs: $RUNS  |  turns/persona: $TURNS"
echo "▶ Root: $ROOT"
echo

for i in $(seq 1 "$RUNS"); do
    SUBDIR="$ROOT/run-$i"
    echo "═══════════════════════════════════════════════"
    echo "  RUN $i / $RUNS  →  $SUBDIR"
    echo "═══════════════════════════════════════════════"
    PERSONAS=(cu-ti tung-trau be-mochi co-hang cu-do anh-canh hai-sat-thu)
    mkdir -p "$SUBDIR"
    for p in "${PERSONAS[@]}"; do
        node test_persona_pipeline.js --persona="$p" --turns="$TURNS" --movetime=200 --out="$SUBDIR/$p.json" > /dev/null
        node test_persona_judge.js "$SUBDIR/$p.json" --out="$SUBDIR/$p-judge.json" > /dev/null
        echo "  ✓ $p"
    done
    node test_persona_aggregate.js "$SUBDIR" --out="$SUBDIR/SUMMARY.md" > /dev/null
    echo "  → $(grep 'Cringe rate' "$SUBDIR/SUMMARY.md")"
    echo
done

echo "▶ Aggregating across $RUNS runs..."
node test_persona_multirun_aggregate.js "$ROOT" --runs="$RUNS" --out="$ROOT/MULTIRUN.md"
echo
echo "✓ Done. Final report: $ROOT/MULTIRUN.md"
