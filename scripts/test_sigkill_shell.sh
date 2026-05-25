#!/bin/sh
# Test signal termination of blocked foreground/background processes via the shell.

PANE="${1:-${PANE:-0:0.0}}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/tmux_test_lib.sh"

echo "=== Test: signal termination of blocked processes ==="

wait_for_prompt

# -------------------------------------------------------------------------
# Ctrl-C kills foreground process blocked in sleep
# -------------------------------------------------------------------------
echo "--- Ctrl-C kills foreground sleep ---"

# Start sleep 100 in foreground, then immediately send Ctrl-C.
tmux send-keys -t "$PANE" "sleep 100" Enter
sleep 0.3
tmux send-keys -t "$PANE" C-c
wait_for_prompt

OUT=$(run_cmd_exitcode "echo exitcode=\$?")
result "Ctrl-C on foreground sleep: shell gets nonzero exit" \
  "$(contains "$OUT" "exitcode=130")"

# -------------------------------------------------------------------------
# Ctrl-C kills foreground process blocked reading a pipe with no writer
# -------------------------------------------------------------------------
echo "--- Ctrl-C kills foreground pipe-read ---"

# cat with no input blocks in read(); Ctrl-C must wake and terminate it.
tmux send-keys -t "$PANE" "cat" Enter
sleep 0.3
tmux send-keys -t "$PANE" C-c
wait_for_prompt

OUT=$(run_cmd_exitcode "echo exitcode=\$?")
result "Ctrl-C on foreground cat (blocked read): shell gets nonzero exit" \
  "$(contains "$OUT" "exitcode=130")"

# -------------------------------------------------------------------------
# SIGKILL (kill -9) on background process blocked in sleep
# -------------------------------------------------------------------------
echo "--- kill -9 kills background sleep ---"

OUT=$(run_cmd_exitcode "sleep 100 & PID=\$! ; kill -9 \$PID ; wait \$PID ; echo exitcode=\$?")
result "kill -9 background sleep: wait reports signal exit (137)" \
  "$(contains "$OUT" "exitcode=137")"

# -------------------------------------------------------------------------
# Shell prompt returns promptly after Ctrl-C (process does not linger)
# -------------------------------------------------------------------------
echo "--- prompt returns quickly after Ctrl-C ---"

tmux send-keys -t "$PANE" "sleep 100" Enter
sleep 0.3
tmux send-keys -t "$PANE" C-c
# wait_for_prompt uses a short poll; if it times out the test fails.
if wait_for_prompt; then
  result "prompt returns after Ctrl-C on sleep 100" 1
else
  result "prompt returns after Ctrl-C on sleep 100" 0
fi

print_summary
