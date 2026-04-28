#!/usr/bin/env bash
# Port-forward vLLM (Qwen 3.6) from internal server to localhost:3073.
# Usage:  ./forward-vllm.sh start | stop | status
set -euo pipefail

REMOTE_USER="vht"
REMOTE_HOST="127.0.0.1"
REMOTE_SSH_PORT=43022
LOCAL_PORT=3073
TARGET="192.168.205.172:3073"
PIDFILE="/tmp/mychess-vllm-forward.pid"

cmd="${1:-status}"

is_running() {
    [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null
}

case "$cmd" in
    start)
        if is_running; then
            echo "Forward already running (PID $(cat "$PIDFILE"))."
            exit 0
        fi
        # -f: background, -N: no remote command, -L: local-forward
        ssh -fN -o ExitOnForwardFailure=yes -o ServerAliveInterval=30 \
            -L "${LOCAL_PORT}:${TARGET}" -p "$REMOTE_SSH_PORT" \
            "${REMOTE_USER}@${REMOTE_HOST}"
        # ssh -f forks; capture pid via lsof on the local port (most portable).
        sleep 0.5
        PID=$(ss -ltnp 2>/dev/null | awk -v p=":$LOCAL_PORT" '$4 ~ p {print $0}' | grep -oP 'pid=\K\d+' | head -1)
        if [ -z "$PID" ]; then
            echo "Forward started but PID not detectable."
        else
            echo "$PID" > "$PIDFILE"
            echo "Forward started (PID $PID): localhost:$LOCAL_PORT -> $TARGET"
        fi
        ;;
    stop)
        if is_running; then
            kill "$(cat "$PIDFILE")" && rm -f "$PIDFILE"
            echo "Stopped."
        else
            # try to kill any matching ssh forward we might have orphaned
            pkill -f "ssh.*-L *${LOCAL_PORT}:${TARGET}" 2>/dev/null || true
            rm -f "$PIDFILE"
            echo "(no PID tracked; killed any matching ssh forward)"
        fi
        ;;
    status)
        if is_running; then
            echo "Running (PID $(cat "$PIDFILE")). localhost:$LOCAL_PORT -> $TARGET"
            curl -s --connect-timeout 3 "http://localhost:${LOCAL_PORT}/v1/models" -o /dev/null \
                -w "  /v1/models HTTP %{http_code}\n" || true
        else
            echo "Not running."
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status}" >&2
        exit 2
        ;;
esac
