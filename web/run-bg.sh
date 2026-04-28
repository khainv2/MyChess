#!/usr/bin/env bash
# Start MyChess web server as a detached background process.
# Used as systemd-fallback on hosts without systemd PID 1.
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

mkdir -p logs

# Refuse to start if already running.
if [ -f logs/server.pid ] && kill -0 "$(cat logs/server.pid)" 2>/dev/null; then
    echo "Already running (PID $(cat logs/server.pid))."
    exit 1
fi

nohup node server.js > logs/server.log 2>&1 &
PID=$!
echo "$PID" > logs/server.pid
disown "$PID" 2>/dev/null || true

# Give it a moment, then verify port is bound.
sleep 1
if ! kill -0 "$PID" 2>/dev/null; then
    echo "Server died on startup. Last log lines:"
    tail -20 logs/server.log
    rm -f logs/server.pid
    exit 1
fi
echo "Started PID $PID"
echo "Logs: $SCRIPT_DIR/logs/server.log"
