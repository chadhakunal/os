#!/bin/sh
# Automate test runs in QEMU via tmux, polling for the shell prompt between tests.
# Usage: ./scripts/run_tests.sh [pane]

PANE="${1:-${PANE:-0:0.0}}"
TIMEOUT=600   # max poll iterations (each 0.5s = ~5 min per test)
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
    # Capture the last non-empty line of the pane
    local last
    last=$(tmux capture-pane -t "$PANE" -p | grep -v '^[[:space:]]*$' | tail -1)
    # Match any line ending with '$ ' or '$ ' at end (the shell prompt)
    if echo "$last" | grep -qE '\$\s*$'; then
      return 0
    fi
    sleep $POLL
    elapsed=$(( elapsed + 1 ))
  done
  echo "  [TIMEOUT] waited ${TIMEOUT}s for prompt — last line was: $last"
  return 1
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Shell-side test scripts (run on host, inject into QEMU via tmux)
SHELL_TESTS="
test_ls
test_shell
test_bins_shell
test_posix
test_sigkill_shell
test_procfs
"

echo "=== running tests on QEMU pane $PANE ==="

# Wait for initial prompt before starting
echo "Waiting for shell prompt..."
wait_for_prompt

send "clear"
sleep 0.3
wait_for_prompt

# Run in-guest test binaries
for t in $TESTS; do
  echo "--- running: $t"
  send "/bin/tests/$t"
  wait_for_prompt
  echo "--- done: $t"
done

send "echo '=== BINARY TESTS DONE ==='"
wait_for_prompt

# Run host-side tmux test scripts
for t in $SHELL_TESTS; do
  echo "--- running shell test: $t"
  PANE="$PANE" sh "$SCRIPT_DIR/${t}.sh"
  echo "--- done: $t"
done

echo "=== all tests complete ==="
