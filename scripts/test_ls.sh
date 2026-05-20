#!/bin/sh
# Test /bin/ls behaviour via tmux capture-pane.

PANE="${1:-${PANE:-0:0.0}}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/tmux_test_lib.sh"

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
OUT=$(run_cmd_raw "ls -a /tmp/lstest/emptydir")
BLANK=$(echo "$OUT" | grep -c '^[[:space:]]*$' || true)
result "ls -a empty dir no blank lines" "$([ "$BLANK" = "0" ] && echo 1 || echo 0)"

# 3. ls on missing dir exits nonzero
OUT=$(run_cmd_exitcode "ls /tmp/lstest/nosuchdir ; echo exitcode=\$?")
result "ls missing dir exits nonzero" "$(contains "$OUT" "exitcode=1")"

# 4. ls -l shows permission bits
OUT=$(run_cmd "ls -l /tmp/lstest")
result "ls -l shows permissions"    "$(matches "$OUT" '^[-d]')"
result "ls -l shows file1.txt"      "$(contains "$OUT" "file1.txt")"
result "ls -l shows subdir as d"    "$(matches "$OUT" '^d')"

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
OUT=$(run_cmd_raw "ls /tmp/lstest")
BLANK=$(echo "$OUT" | grep -c '^[[:space:]]*$' || true)
result "ls no blank lines in output" "$([ "$BLANK" = "0" ] && echo 1 || echo 0)"

# Cleanup
send "rm -rf /tmp/lstest"
wait_for_prompt

print_summary
