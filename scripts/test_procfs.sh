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
PID=$(run_cmd "cat /proc/self/status" | grep '^Pid:' | sed 's/Pid:[[:space:]]*//')
OUT=$(run_cmd "ls /proc")
result "/proc lists own PID dir"    "$(contains "$OUT" "$PID")"

# ------------------------------------------------------------------ #
# /proc/uptime                                                        #
# ------------------------------------------------------------------ #
echo "--- /proc/uptime"

OUT=$(run_cmd "cat /proc/uptime")
# Grab just the first non-empty line; strip carriage returns from tmux capture
UP_LINE=$(echo "$OUT" | grep -m1 '[0-9]' | tr -d '\r')
result "uptime: non-empty"                  "$([ -n "$UP_LINE" ] && echo 1 || echo 0)"
result "uptime: matches SECS.CS IDLE.CS"    "$(matches "$UP_LINE" '^[0-9]+\.[0-9]+ [0-9]+\.[0-9]+')"
result "uptime: has two fields"             "$(echo "$UP_LINE" | grep -qE '^[0-9]+\.[0-9]+ [0-9]+\.[0-9]+$' && echo 1 || echo 0)"

# centiseconds must be 0-99 — extract digits after first dot, before first space
UP_CS=$(echo "$UP_LINE" | tr -d '\r' | sed 's/[0-9]*\.\([0-9]*\) .*/\1/')
result "uptime: centiseconds <= 99"         "$([ "$UP_CS" -le 99 ] 2>/dev/null && echo 1 || echo 0)"

# two reads: second >= first (integer seconds before the dot)
UP1=$(run_cmd "cat /proc/uptime" | grep -m1 '[0-9]' | tr -d '\r' | sed 's/\..*//')
UP2=$(run_cmd "cat /proc/uptime" | grep -m1 '[0-9]' | tr -d '\r' | sed 's/\..*//')
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

TOTAL=$(echo "$OUT" | grep '^MemTotal:' | tr -d '\r' | sed 's/MemTotal:[[:space:]]*//' | sed 's/ kB//')
FREE=$(echo  "$OUT" | grep '^MemFree:'  | tr -d '\r' | sed 's/MemFree:[[:space:]]*//'  | sed 's/ kB//')
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

STATE=$(echo   "$OUT" | grep -m1 '^State:'   | tr -d '\r' | sed 's/State:[[:space:]]*//' | cut -c1)
PGRP=$(echo    "$OUT" | grep -m1 '^Pgrp:'   | tr -d '\r' | sed 's/Pgrp:[[:space:]]*//')
SID=$(echo     "$OUT" | grep -m1 '^Sid:'    | tr -d '\r' | sed 's/Sid:[[:space:]]*//')
THREADS=$(echo "$OUT" | grep -m1 '^Threads:'| tr -d '\r' | sed 's/Threads:[[:space:]]*//')
VMSIZE=$(echo  "$OUT" | grep -m1 '^VmSize:' | tr -d '\r' | sed 's/VmSize:[[:space:]]*//' | sed 's/ kB//')
VMRSS=$(echo   "$OUT" | grep -m1 '^VmRSS:'  | tr -d '\r' | sed 's/VmRSS:[[:space:]]*//'  | sed 's/ kB//')
result "status: State is valid letter"      "$(echo "$STATE" | grep -qE '^[RSDTZ]$' && echo 1 || echo 0)"
result "status: Pgrp >= 1"                  "$([ "$PGRP"    -ge 1 ] 2>/dev/null && echo 1 || echo 0)"
result "status: Sid >= 1"                   "$([ "$SID"     -ge 1 ] 2>/dev/null && echo 1 || echo 0)"
result "status: Threads >= 1"               "$([ "$THREADS" -ge 1 ] 2>/dev/null && echo 1 || echo 0)"
result "status: VmSize > 0"                 "$([ "$VMSIZE"  -gt 0 ] 2>/dev/null && echo 1 || echo 0)"
result "status: VmRSS <= VmSize"            "$([ "$VMRSS"  -le "$VMSIZE" ] 2>/dev/null && echo 1 || echo 0)"

# ------------------------------------------------------------------ #
# /proc/<pid>/status — Pid/PPid match shell's values                 #
# ------------------------------------------------------------------ #
echo "--- /proc/self/status pid consistency"

# cat itself: its Pid should match what the shell sees
OUT=$(run_cmd "cat /proc/self/status")
RPID=$(echo  "$OUT" | grep -m1 '^Pid:'  | tr -d '\r' | sed 's/Pid:[[:space:]]*//')
RPPID=$(echo "$OUT" | grep -m1 '^PPid:' | tr -d '\r' | sed 's/PPid:[[:space:]]*//')
result "status: Pid is numeric and > 0"     "$(echo "$RPID"  | grep -qE '^[0-9]+$' && [ "$RPID"  -gt 0 ] && echo 1 || echo 0)"
result "status: PPid is numeric and >= 1"   "$(echo "$RPPID" | grep -qE '^[0-9]+$' && [ "$RPPID" -ge 1 ] && echo 1 || echo 0)"

# ------------------------------------------------------------------ #
# /proc/self/exe                                                      #
# ------------------------------------------------------------------ #
echo "--- /proc/self/exe"

OUT=$(run_cmd "readlink /proc/self/exe")
result "exe: readlink non-empty"            "$([ -n "$OUT" ] && echo 1 || echo 0)"
result "exe: starts with /"                 "$(matches "$OUT" '^/')"
result "exe: contains /bin/"                "$(contains "$OUT" "/bin/")"

OUT=$(run_cmd "readlink /proc/1/exe")
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
echo "--- /proc/self/cwd"

OUT=$(run_cmd "readlink /proc/self/cwd")
result "cwd: readlink non-empty"            "$([ -n "$OUT" ] && echo 1 || echo 0)"
result "cwd: starts with /"                 "$(matches "$OUT" '^/')"

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
