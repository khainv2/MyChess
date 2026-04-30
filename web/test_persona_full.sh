#!/usr/bin/env bash
# Full test runner: 7 persona × pipeline + judge + aggregate.
# Sequential để tránh overload vLLM.
set -e
cd "$(dirname "$0")"

PERSONAS=(cu-ti tung-trau be-mochi co-hang cu-do anh-canh hai-sat-thu)
TURNS=${TURNS:-30}
RUN_ID=$(date +%Y%m%d-%H%M%S)
LOG_DIR=logs/full-$RUN_ID
mkdir -p "$LOG_DIR"

echo "▶ Run ID: $RUN_ID  |  turns/persona: $TURNS  |  logs: $LOG_DIR"
echo

for p in "${PERSONAS[@]}"; do
    echo "═══ Pipeline: $p ═══"
    node test_persona_pipeline.js --persona="$p" --turns="$TURNS" --movetime=200 --out="$LOG_DIR/$p.json"
    echo
    echo "═══ Judge: $p ═══"
    node test_persona_judge.js "$LOG_DIR/$p.json" --out="$LOG_DIR/$p-judge.json"
    echo
done

echo "▶ Aggregating..."
node test_persona_aggregate.js "$LOG_DIR" --out="$LOG_DIR/SUMMARY.md"
echo
echo "✓ Done. Reports:"
ls -lh "$LOG_DIR"/
