#!/bin/sh
# Test /bin/ls behaviour via tmux capture-pane.

PANE="${1:-${PANE:-0:0.0}}"
TIMEOUT=10
POLL=0.3

passed=0
failed=0

send() {
  tmux send-keys -t "$PANE" "$1" Enter
}

wait_for_prompt() {
  local elapsed=0
  while [ $elapsed -lt $TIMEOUT ]; do
    local last
    last=$(tmux capture-pane -t "$PANE" -p | grep -v '^[[:space:]]*$' | tail -1)
    if echo "$last" | grep -qE '\$\s*$'; then
      return 0
    fi
    sleep $POLL
    elapsed=$(( elapsed + 1 ))
  done
  return 1
}

# Capture the output of the last command (everything between the command line
# and the next prompt).
capture_output() {
  tmux capture-pane -t "$PANE" -p \
    | grep -v '^[[:space:]]*$' \
    | grep -v '\$' \
    | tail -n +2
}

# Run a command and return its output lines via stdout.
run_cmd() {
  send "$1"
  wait_for_prompt
  # Grab pane, strip prompt lines, strip the command echo line
  tmux capture-pane -t "$PANE" -p \
    | sed '/^\s*$/d' \
    | grep -v '^\s*$' \
    | grep -v '\$ ' \
    | grep -v '^$'
}

result() {
  local name="$1" ok="$2"
  if [ "$ok" = "1" ]; then
    echo "  [PASS] $name"
    passed=$(( passed + 1 ))
  else
    echo "  [FAIL] $name"
    failed=$(( failed + 1 ))
  fi
}

contains() {
  echo "$1" | grep -qF "$2"
}

not_contains() {
  ! echo "$1" | grep -qF "$2"
}

echo "=== Test: ls ==="

wait_for_prompt

# Setup: create known files in /tmp
send "rm -rf /tmp/lstest"
wait_for_prompt
send "mkdir -p /tmp/lstest/subdir"
wait_for_prompt
send "echo hello > /tmp/lstest/file1.txt"
wait_for_prompt
send "echo world > /tmp/lstest/file2.txt"
wait_for_prompt

# 1. ls lists files
OUT=$(run_cmd "ls /tmp/lstest")
result "ls lists file1.txt" "$(contains "$OUT" "file1.txt" && echo 1 || echo 0)"
result "ls lists file2.txt" "$(contains "$OUT" "file2.txt" && echo 1 || echo 0)"
result "ls lists subdir"    "$(contains "$OUT" "subdir"    && echo 1 || echo 0)"

# 2. ls does not show dotfiles by default
result "ls hides dotfiles by default" "$(not_contains "$OUT" "." && echo 1 || echo 0)"

# 3. ls -a shows dot and dotdot
OUT=$(run_cmd "ls -a /tmp/lstest")
result "ls -a shows ."  "$(contains "$OUT" "."  && echo 1 || echo 0)"
result "ls -a shows .." "$(contains "$OUT" ".." && echo 1 || echo 0)"

# 4. ls -a shows no blank lines (our sbfs fix)
BLANK=$(echo "$OUT" | grep -c '^$')
result "ls -a no blank lines" "$([ "$BLANK" = "0" ] && echo 1 || echo 0)"

# 5. ls on missing dir exits nonzero
send "ls /tmp/lstest/nosuchdir ; echo exitcode=\$?"
wait_for_prompt
OUT=$(tmux capture-pane -t "$PANE" -p | grep "exitcode=")
result "ls missing dir exits nonzero" "$(contains "$OUT" "exitcode=1" && echo 1 || echo 0)"

# 6. ls -l shows permissions and size
OUT=$(run_cmd "ls -l /tmp/lstest")
result "ls -l shows permissions" "$(echo "$OUT" | grep -qE '^[-d]' && echo 1 || echo 0)"

# 7. ls with no args lists current directory
send "cd /tmp/lstest"
wait_for_prompt
OUT=$(run_cmd "ls")
result "ls with no args lists cwd" "$(contains "$OUT" "file1.txt" && echo 1 || echo 0)"
send "cd /"
wait_for_prompt

# 8. ls on a single file just prints the filename
OUT=$(run_cmd "ls /tmp/lstest/file1.txt")
result "ls on a file prints its name" "$(contains "$OUT" "file1.txt" && echo 1 || echo 0)"

# 9. ls multiple paths
OUT=$(run_cmd "ls /tmp/lstest/file1.txt /tmp/lstest/file2.txt")
result "ls multiple paths shows both" \
  "$(contains "$OUT" "file1.txt" && contains "$OUT" "file2.txt" && echo 1 || echo 0)"

# Cleanup
send "rm -rf /tmp/lstest"
wait_for_prompt

echo ""
echo "$passed passed, $failed failed"
[ "$failed" = "0" ]
