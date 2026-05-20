#!/bin/sh
# Send commands to the OS running in QEMU via tmux and capture output.

PANE="${PANE:-0:0.0}"
DELAY="${DELAY:-0.3}"
LOG="/tmp/qemu_test_output.txt"

send() {
  tmux send-keys -t "$PANE" "$1" Enter
  sleep "$DELAY"
}

# Capture output by redirecting into a file on the guest
run_test() {
  local bin="$1"
  local outfile="/tmp/test_out.txt"

  send "$bin > $outfile 2>&1"
  sleep 1
  send "cat $outfile"
  sleep 0.5
}

echo "=== Sending tests to QEMU pane $PANE ==="

# Clear screen first
send "clear"
sleep 0.5

# Basic sanity
send "echo '=== starting tests ==='"

# Run each test binary
run_test "/bin/test_bins"
run_test "/bin/testsignal"
run_test "/bin/test_syscalls"
run_test "/bin/test_shell"
run_test "/bin/test_pipe"
run_test "/bin/test_mmap"

send "echo '=== done ==='"

echo "Commands sent. Watch pane $PANE for output."
echo "To capture output to a file, run:"
echo "  tmux pipe-pane -t $PANE 'cat >> $LOG'"
echo "before running this script."
