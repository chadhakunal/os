#!/bin/sh
# Test signal termination of blocked foreground/background processes via the shell.

PANE="${1:-${PANE:-0:0.0}}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/tmux_test_lib.sh"

echo "=== Test: signal termination of blocked processes ==="

wait_for_prompt

# -------------------------------------------------------------------------
# Ctrl-C kills foreground process blocked in sleep
# Shell must report $? = 130 (128 + SIGINT)
# -------------------------------------------------------------------------
echo "--- Ctrl-C kills foreground sleep ---"

tmux send-keys -t "$PANE" "sleep 100" Enter
sleep 0.3
tmux send-keys -t "$PANE" C-c
wait_for_prompt

OUT=$(run_cmd_exitcode "echo exitcode=\$?")
result "Ctrl-C on foreground sleep: \$? is 130" \
  "$(contains "$OUT" "exitcode=130")"

# -------------------------------------------------------------------------
# Ctrl-C kills foreground process blocked reading from a pipe with no writer
# -------------------------------------------------------------------------
echo "--- Ctrl-C kills foreground cat (blocked read) ---"

tmux send-keys -t "$PANE" "cat" Enter
sleep 0.3
tmux send-keys -t "$PANE" C-c
wait_for_prompt

OUT=$(run_cmd_exitcode "echo exitcode=\$?")
result "Ctrl-C on foreground cat (blocked read): \$? is 130" \
  "$(contains "$OUT" "exitcode=130")"

# -------------------------------------------------------------------------
# SIGKILL on background sleep — wait must report 137 (128 + SIGKILL)
# -------------------------------------------------------------------------
echo "--- kill -9 background sleep ---"

OUT=$(run_cmd_exitcode "sleep 100 & kill -9 \$! ; wait \$! ; echo exitcode=\$?")
result "kill -9 background sleep: wait reports 137" \
  "$(contains "$OUT" "exitcode=137")"

# -------------------------------------------------------------------------
# Prompt returns promptly after Ctrl-C (process does not linger)
# -------------------------------------------------------------------------
echo "--- prompt returns quickly after Ctrl-C ---"

tmux send-keys -t "$PANE" "sleep 100" Enter
sleep 0.3
tmux send-keys -t "$PANE" C-c
if wait_for_prompt; then
  result "prompt returns after Ctrl-C on sleep 100" 1
else
  result "prompt returns after Ctrl-C on sleep 100" 0
fi

print_summary
