#!/bin/sh
# Test /bin/ls behaviour via tmux capture-pane.

PANE="${1:-${PANE:-0:0.0}}"
TIMEOUT=30   # max poll iterations (each ~0.3s = ~9 seconds total)
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

# Run a command and capture its output using sentinel markers.
# Prints a unique BEGIN marker, runs the command, prints END marker,
# then extracts only the lines between those two markers.
_seq=0
run_cmd() {
  local cmd="$1"
  _seq=$(( _seq + 1 ))
  local begin="__BEGIN_${_seq}__"
  local end="__END_${_seq}__"

  # Send begin marker, then command, then end marker as three separate commands
  send "echo $begin"
  wait_for_prompt
  send "$cmd"
  wait_for_prompt
  send "echo $end"
  wait_for_prompt

  # Extract lines strictly between begin and end markers
  tmux capture-pane -t "$PANE" -p \
    | awk "/^${begin}$/{found=1; next} /^${end}$/{found=0} found && !/\\\$ /" \
    | grep -v '^\s*$'
}

# Like run_cmd but also captures the exit code via a sentinel echo.
run_cmd_exitcode() {
  local cmd="$1"
  _seq=$(( _seq + 1 ))
  local begin="__BEGIN_${_seq}__"
  local end="__END_${_seq}__"

  send "echo $begin"
  wait_for_prompt
  send "$cmd"
  wait_for_prompt
  send "echo $end"
  wait_for_prompt

  tmux capture-pane -t "$PANE" -p \
    | awk "/^${begin}$/{found=1; next} /^${end}$/{found=0} found"
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

contains() { echo "$1" | grep -qF "$2" && echo 1 || echo 0; }
not_contains() { echo "$1" | grep -qF "$2" && echo 0 || echo 1; }

echo "=== Test: ls ==="

wait_for_prompt

# Setup
send "rm -rf /tmp/lstest && mkdir -p /tmp/lstest/subdir && echo hello > /tmp/lstest/file1.txt && echo world > /tmp/lstest/file2.txt"
wait_for_prompt

# 1. ls lists expected files
OUT=$(run_cmd "ls /tmp/lstest")
result "ls lists file1.txt"         "$(contains "$OUT" "file1.txt")"
result "ls lists file2.txt"         "$(contains "$OUT" "file2.txt")"
result "ls lists subdir"            "$(contains "$OUT" "subdir")"

# 2. ls -a on empty dir has no blank lines (sbfs zeroed block fix)
send "mkdir -p /tmp/lstest/emptydir"
wait_for_prompt
OUT=$(run_cmd "ls -a /tmp/lstest/emptydir")
BLANK=$(echo "$OUT" | grep -c '^[[:space:]]*$' || true)
result "ls -a empty dir no blank lines" "$([ "$BLANK" = "0" ] && echo 1 || echo 0)"

# 3. ls on missing dir exits nonzero
OUT=$(run_cmd_exitcode "ls /tmp/lstest/nosuchdir ; echo exitcode=\$?")
result "ls missing dir exits nonzero" "$(contains "$OUT" "exitcode=1")"

# 4. ls -l shows permission bits
OUT=$(run_cmd "ls -l /tmp/lstest")
result "ls -l shows permissions"    "$(echo "$OUT" | grep -qE '^[-d]' && echo 1 || echo 0)"
result "ls -l shows file1.txt"      "$(contains "$OUT" "file1.txt")"
result "ls -l shows subdir as d"    "$(echo "$OUT" | grep -q '^d' && echo 1 || echo 0)"

# 5. ls with no args lists cwd
send "cd /tmp/lstest"
wait_for_prompt
OUT=$(run_cmd "ls")
result "ls no args lists cwd"       "$(contains "$OUT" "file1.txt")"
send "cd /"
wait_for_prompt

# 6. ls on a single file prints its name
OUT=$(run_cmd "ls /tmp/lstest/file1.txt")
result "ls single file prints name" "$(contains "$OUT" "file1.txt")"

# 7. ls multiple paths
OUT=$(run_cmd "ls /tmp/lstest/file1.txt /tmp/lstest/file2.txt")
result "ls multiple paths"          "$([ "$(contains "$OUT" "file1.txt")" = "1" ] && [ "$(contains "$OUT" "file2.txt")" = "1" ] && echo 1 || echo 0)"

# 8. ls output has no blank lines for normal dir
OUT=$(run_cmd "ls /tmp/lstest")
BLANK=$(echo "$OUT" | grep -c '^[[:space:]]*$' || true)
result "ls no blank lines in output" "$([ "$BLANK" = "0" ] && echo 1 || echo 0)"

# Cleanup
send "rm -rf /tmp/lstest"
wait_for_prompt

echo ""
echo "$passed passed, $failed failed"
[ "$failed" = "0" ]
