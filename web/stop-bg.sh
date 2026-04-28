#!/usr/bin/env bash
# Stop the detached MyChess web server.
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f logs/server.pid ]; then
    echo "Not running (no PID file)."
    exit 0
fi
PID="$(cat logs/server.pid)"
if kill -0 "$PID" 2>/dev/null; then
    kill "$PID"
    sleep 1
    if kill -0 "$PID" 2>/dev/null; then
        kill -9 "$PID" 2>/dev/null || true
    fi
    echo "Stopped PID $PID"
else
    echo "PID $PID was already gone."
fi
rm -f logs/server.pid
