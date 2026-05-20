#!/bin/sh
# Shared library for tmux-based QEMU shell tests.
# Source this file; set PANE before sourcing or pass pane as $1.

PANE="${PANE:-0:0.0}"
TIMEOUT=60    # max poll iterations per wait_for_prompt (~18s at 0.3s each)
POLL=0.3

passed=0
failed=0
_seq=0

send() {
  tmux send-keys -t "$PANE" "$1" Enter
}

wait_for_prompt() {
  local elapsed=0
  while [ $elapsed -lt $TIMEOUT ]; do
    local last
    last=$(tmux capture-pane -t "$PANE" -p -S -3 | grep -v '^[[:space:]]*$' | tail -1)
    if echo "$last" | grep -qE '\$\s*$'; then
      return 0
    fi
    sleep $POLL
    elapsed=$(( elapsed + 1 ))
  done
  echo "  [TIMEOUT] waited for prompt; last line: $last"
  return 1
}

# run_cmd CMD
# Sends CMD to the pane, waits for prompt, returns stdout between sentinel markers.
# Strips blank lines and prompt lines from output.
run_cmd() {
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

  tmux capture-pane -t "$PANE" -p -S - \
    | awk "/^${begin}$/{found=1; next} /^${end}$/{found=0} found && !/\\\$ /" \
    | grep -v '^\s*$'
}

# run_cmd_raw CMD
# Like run_cmd but does NOT strip blank lines (for testing blank line counts etc.)
run_cmd_raw() {
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

  tmux capture-pane -t "$PANE" -p -S - \
    | awk "/^${begin}$/{found=1; next} /^${end}$/{found=0} found && !/\\\$ /"
}

# run_cmd_exitcode CMD
# Like run_cmd but captures raw output including exit codes — does not strip blanks.
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

  tmux capture-pane -t "$PANE" -p -S - \
    | awk "/^${begin}$/{found=1; next} /^${end}$/{found=0} found"
}

result() {
  local name="$1" ok="$2"
  if [ "$ok" = "1" ]; then
    printf "  [PASS] %s\n" "$name"
    passed=$(( passed + 1 ))
  else
    printf "  [FAIL] %s\n" "$name"
    failed=$(( failed + 1 ))
  fi
}

contains()     { echo "$1" | grep -qF "$2"  && echo 1 || echo 0; }
not_contains() { echo "$1" | grep -qF "$2"  && echo 0 || echo 1; }
matches()      { echo "$1" | grep -qE "$2"  && echo 1 || echo 0; }
equals()       { [ "$1" = "$2" ]             && echo 1 || echo 0; }

print_summary() {
  echo ""
  echo "$passed passed, $failed failed"
  [ "$failed" = "0" ]
}
