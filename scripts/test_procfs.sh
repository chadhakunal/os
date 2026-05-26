#!/bin/sh
# Test /proc filesystem via tmux capture-pane.

PANE="${1:-${PANE:-0:0.0}}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/tmux_test_lib.sh"

echo "=== Test: procfs ==="

wait_for_prompt

# ------------------------------------------------------------------ #
# /proc directory listing                                             #
# ------------------------------------------------------------------ #
echo "--- /proc directory"

OUT=$(run_cmd "ls /proc")
result "/proc lists uptime"         "$(contains "$OUT" "uptime")"
result "/proc lists meminfo"        "$(contains "$OUT" "meminfo")"
result "/proc lists self"           "$(contains "$OUT" "self")"

# Our own PID should appear
PID=$(run_cmd "cat /proc/self/status" | grep '^Pid:' | tr -s ' \t' ' ' | cut -d' ' -f2)
OUT=$(run_cmd "ls /proc")
result "/proc lists own PID dir"    "$(contains "$OUT" "$PID")"

# ------------------------------------------------------------------ #
# /proc/uptime                                                        #
# ------------------------------------------------------------------ #
echo "--- /proc/uptime"

OUT=$(run_cmd "cat /proc/uptime")
# Grab just the first non-empty line in case run_cmd captures a trailing newline
UP_LINE=$(echo "$OUT" | grep -m1 '[0-9]')
result "uptime: non-empty"                  "$([ -n "$UP_LINE" ] && echo 1 || echo 0)"
result "uptime: matches SECS.CS IDLE.CS"    "$(matches "$UP_LINE" '^[0-9]+\.[0-9]+ [0-9]+\.[0-9]+')"
result "uptime: has two fields"             "$([ "$(echo "$UP_LINE" | awk '{print NF}')" = "2" ] && echo 1 || echo 0)"

# centiseconds must be 0-99
UP_CS=$(echo "$UP_LINE" | awk -F'[. ]' '{print $2}')
result "uptime: centiseconds <= 99"         "$([ "$UP_CS" -le 99 ] 2>/dev/null && echo 1 || echo 0)"

# two reads: second >= first (integer seconds)
UP1=$(run_cmd "cat /proc/uptime" | grep -m1 '[0-9]' | awk -F. '{print $1}')
UP2=$(run_cmd "cat /proc/uptime" | grep -m1 '[0-9]' | awk -F. '{print $1}')
result "uptime: monotonically non-decreasing" "$([ "$UP2" -ge "$UP1" ] 2>/dev/null && echo 1 || echo 0)"

# ------------------------------------------------------------------ #
# /proc/meminfo                                                       #
# ------------------------------------------------------------------ #
echo "--- /proc/meminfo"

OUT=$(run_cmd "cat /proc/meminfo")
result "meminfo: contains MemTotal"         "$(contains "$OUT" "MemTotal")"
result "meminfo: contains MemFree"          "$(contains "$OUT" "MemFree")"
result "meminfo: contains MemAvailable"     "$(contains "$OUT" "MemAvailable")"
result "meminfo: MemTotal ends with kB"     "$(matches "$OUT" 'MemTotal:.*kB')"
result "meminfo: MemFree ends with kB"      "$(matches "$OUT" 'MemFree:.*kB')"

TOTAL=$(echo "$OUT" | grep '^MemTotal:' | tr -s ' \t' ' ' | cut -d' ' -f2)
FREE=$(echo  "$OUT" | grep '^MemFree:'  | tr -s ' \t' ' ' | cut -d' ' -f2)
result "meminfo: MemTotal > 0"              "$([ "$TOTAL" -gt 0 ] 2>/dev/null && echo 1 || echo 0)"
result "meminfo: MemFree >= 0"             "$([ "$FREE"  -ge 0 ] 2>/dev/null && echo 1 || echo 0)"
result "meminfo: MemFree <= MemTotal"       "$([ "$FREE" -le "$TOTAL" ] 2>/dev/null && echo 1 || echo 0)"
result "meminfo: MemTotal > 1024 kB"        "$([ "$TOTAL" -gt 1024 ] 2>/dev/null && echo 1 || echo 0)"

# ------------------------------------------------------------------ #
# /proc/self/status                                                   #
# ------------------------------------------------------------------ #
echo "--- /proc/self/status"

OUT=$(run_cmd "cat /proc/self/status")
result "status: non-empty"                  "$([ -n "$OUT" ] && echo 1 || echo 0)"
result "status: contains Name"              "$(contains "$OUT" "Name:")"
result "status: contains Pid"               "$(contains "$OUT" "Pid:")"
result "status: contains PPid"              "$(contains "$OUT" "PPid:")"
result "status: contains State"             "$(contains "$OUT" "State:")"
result "status: contains Pgrp (not NSpgid)" "$(contains "$OUT" "Pgrp:")"
result "status: contains Sid (not NSsid)"   "$(contains "$OUT" "Sid:")"
result "status: contains Uid"               "$(contains "$OUT" "Uid:")"
result "status: contains Threads"           "$(contains "$OUT" "Threads:")"
result "status: contains VmSize"            "$(contains "$OUT" "VmSize:")"
result "status: contains VmRSS"             "$(contains "$OUT" "VmRSS:")"
result "status: contains SigPnd"            "$(contains "$OUT" "SigPnd:")"
result "status: VmSize ends with kB"        "$(matches "$OUT" 'VmSize:.*kB')"
result "status: VmRSS ends with kB"         "$(matches "$OUT" 'VmRSS:.*kB')"
result "status: Umask is 4 octal digits"    "$(matches "$OUT" 'Umask:[[:space:]]*[0-7][0-7][0-7][0-7]')"

STATE=$(echo "$OUT" | grep '^State:' | tr -s ' \t' ' ' | cut -d' ' -f2)
result "status: State is valid letter"      "$(echo "$STATE" | grep -qE '^[RSDTZ]$' && echo 1 || echo 0)"

PGRP=$(echo "$OUT" | grep '^Pgrp:' | tr -s ' \t' ' ' | cut -d' ' -f2)
SID=$(echo   "$OUT" | grep '^Sid:'  | tr -s ' \t' ' ' | cut -d' ' -f2)
result "status: Pgrp >= 1"                  "$([ "$PGRP" -ge 1 ] 2>/dev/null && echo 1 || echo 0)"
result "status: Sid >= 1"                   "$([ "$SID"  -ge 1 ] 2>/dev/null && echo 1 || echo 0)"

THREADS=$(echo "$OUT" | grep '^Threads:' | tr -s ' \t' ' ' | cut -d' ' -f2)
result "status: Threads >= 1"               "$([ "$THREADS" -ge 1 ] 2>/dev/null && echo 1 || echo 0)"

VMSIZE=$(echo "$OUT" | grep '^VmSize:' | tr -s ' \t' ' ' | cut -d' ' -f2)
VMRSS=$(echo  "$OUT" | grep '^VmRSS:'  | tr -s ' \t' ' ' | cut -d' ' -f2)
result "status: VmSize > 0"                 "$([ "$VMSIZE" -gt 0 ] 2>/dev/null && echo 1 || echo 0)"
result "status: VmRSS <= VmSize"            "$([ "$VMRSS" -le "$VMSIZE" ] 2>/dev/null && echo 1 || echo 0)"

# ------------------------------------------------------------------ #
# /proc/<pid>/status — Pid/PPid match shell's values                 #
# ------------------------------------------------------------------ #
echo "--- /proc/self/status pid consistency"

# cat itself: its Pid should match what the shell sees
OUT=$(run_cmd "cat /proc/self/status")
RPID=$(echo "$OUT" | grep '^Pid:'  | tr -s ' \t' ' ' | cut -d' ' -f2)
result "status: Pid is numeric and > 0"     "$(echo "$RPID" | grep -qE '^[0-9]+$' && [ "$RPID" -gt 0 ] && echo 1 || echo 0)"

RPPID=$(echo "$OUT" | grep '^PPid:' | tr -s ' \t' ' ' | cut -d' ' -f2)
result "status: PPid is numeric and >= 1"   "$(echo "$RPPID" | grep -qE '^[0-9]+$' && [ "$RPPID" -ge 1 ] && echo 1 || echo 0)"

# ------------------------------------------------------------------ #
# /proc/self/exe                                                      #
# ------------------------------------------------------------------ #
echo "--- /proc/self/exe"

OUT=$(run_cmd "cat /proc/self/exe")
result "exe: non-empty"                     "$([ -n "$OUT" ] && echo 1 || echo 0)"
result "exe: starts with /"                 "$(matches "$OUT" '^/')"
result "exe: no trailing newline garbage"   "$([ "$(echo "$OUT" | wc -l)" -le 1 ] && echo 1 || echo 0)"

# /proc/1/exe should be init
OUT=$(run_cmd "cat /proc/1/exe")
result "exe: /proc/1/exe non-empty"         "$([ -n "$OUT" ] && echo 1 || echo 0)"
result "exe: /proc/1/exe starts with /"     "$(matches "$OUT" '^/')"
result "exe: /proc/1/exe contains 'init'"   "$(contains "$OUT" "init")"

# ------------------------------------------------------------------ #
# /proc/self/maps                                                     #
# ------------------------------------------------------------------ #
echo "--- /proc/self/maps"

OUT=$(run_cmd "cat /proc/self/maps")
result "maps: non-empty"                    "$([ -n "$OUT" ] && echo 1 || echo 0)"
result "maps: lines have addr-addr format"  "$(matches "$OUT" '^[0-9a-f]+-[0-9a-f]+')"
result "maps: lines have perms field"       "$(matches "$OUT" '^[0-9a-f]+-[0-9a-f]+ [rwxp-]+')"
result "maps: has executable mapping"       "$(matches "$OUT" 'r.xp')"
result "maps: file-backed line has path"    "$(matches "$OUT" 'r.xp.*/bin/')"
result "maps: path is not truncated"        "$(matches "$OUT" '/bin/[a-z][a-z]')"  # at least 2 chars after /bin/

# /proc/1/maps — init
OUT=$(run_cmd "cat /proc/1/maps")
result "maps: /proc/1/maps non-empty"       "$([ -n "$OUT" ] && echo 1 || echo 0)"
result "maps: /proc/1/maps has /bin/init"   "$(contains "$OUT" "/bin/init")"

# ------------------------------------------------------------------ #
# /proc/<pid>/cmdline                                                 #
# ------------------------------------------------------------------ #
echo "--- /proc/self/cmdline"

OUT=$(run_cmd "cat /proc/self/cmdline")
result "cmdline: non-empty"                 "$([ -n "$OUT" ] && echo 1 || echo 0)"

# ------------------------------------------------------------------ #
# /proc/<pid> directory contents                                      #
# ------------------------------------------------------------------ #
echo "--- /proc/<pid> directory"

OUT=$(run_cmd "ls /proc/self")
result "pid dir: contains status"           "$(contains "$OUT" "status")"
result "pid dir: contains maps"             "$(contains "$OUT" "maps")"
result "pid dir: contains cmdline"          "$(contains "$OUT" "cmdline")"
result "pid dir: contains exe"              "$(contains "$OUT" "exe")"
result "pid dir: contains fd"               "$(contains "$OUT" "fd")"
result "pid dir: contains stat"             "$(contains "$OUT" "stat")"

# ------------------------------------------------------------------ #
# /proc/<pid>/stat — single line format for ps                        #
# ------------------------------------------------------------------ #
echo "--- /proc/self/stat"

OUT=$(run_cmd "cat /proc/self/stat")
result "stat: non-empty"                    "$([ -n "$OUT" ] && echo 1 || echo 0)"
result "stat: starts with pid"              "$(matches "$OUT" '^[0-9]+')"
result "stat: has comm in parens"           "$(matches "$OUT" '^[0-9]+ \([^)]+\)')"
result "stat: state field present"          "$(matches "$OUT" '^[0-9]+ \([^)]+\) [RSDTZ]')"

# ------------------------------------------------------------------ #
# Error cases                                                         #
# ------------------------------------------------------------------ #
echo "--- error cases"

OUT=$(run_cmd_exitcode "cat /proc/0/status ; echo exit=\$?")
result "/proc/0/status fails"               "$(contains "$OUT" "exit=1")"

OUT=$(run_cmd_exitcode "cat /proc/999999/status ; echo exit=\$?")
result "/proc/999999/status fails"          "$(contains "$OUT" "exit=1")"

# procfs files are read-only
OUT=$(run_cmd_exitcode "echo x > /proc/uptime ; echo exit=\$?")
result "/proc/uptime not writable"          "$(contains "$OUT" "exit=1")"

# ------------------------------------------------------------------ #
# Summary                                                             #
# ------------------------------------------------------------------ #
print_summary
