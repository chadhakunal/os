#!/bin/sh
# Automate test runs in QEMU via tmux, polling for the shell prompt between tests.
# Usage: ./scripts/run_tests.sh [pane]

PANE="${1:-${PANE:-0:0.0}}"
TIMEOUT=300   # max seconds to wait per test before giving up
POLL=0.5      # seconds between prompt checks

TESTS="
test_shell
test_signals
test_rlimit
posix_compliance
testjobs
testsignal
test_pipe
test_devnull
test_oom
test_oom_vs_enomem
test_oom_stress
test_bins
test_waitpid
"

send() {
  tmux send-keys -t "$PANE" "$1" Enter
}

# Wait until the last line of the pane ends with '$ ' (the shell prompt).
wait_for_prompt() {
  local elapsed=0
  while [ $elapsed -lt $TIMEOUT ]; do
    # Capture the last line of the pane
    local last
    last=$(tmux capture-pane -t "$PANE" -p | grep -v '^$' | tail -1)
    case "$last" in
      *'$ ') return 0 ;;
    esac
    sleep $POLL
    elapsed=$(( elapsed + 1 ))
  done
  echo "  [TIMEOUT] waited ${TIMEOUT}s for prompt after test — continuing anyway"
  return 1
}

echo "=== running tests on QEMU pane $PANE ==="

# Wait for initial prompt before starting
echo "Waiting for shell prompt..."
wait_for_prompt

send "clear"
sleep 0.3
wait_for_prompt

for t in $TESTS; do
  echo "--- running: $t"
  send "/bin/tests/$t"
  wait_for_prompt
  echo "--- done: $t"
done

send "echo '=== ALL TESTS DONE ==='"
wait_for_prompt

echo "=== all tests complete ==="
