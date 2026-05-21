#!/bin/sh
# Test interactive shell features via tmux capture-pane.

PANE="${1:-${PANE:-0:0.0}}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/tmux_test_lib.sh"

echo "=== Test: shell features ==="

wait_for_prompt

# -------------------------------------------------------------------------
# Redirection
# -------------------------------------------------------------------------
echo "--- redirection ---"

OUT=$(run_cmd "echo hello > /tmp/sh_redir && cat /tmp/sh_redir")
result "stdout redirect > creates file"    "$(contains "$OUT" "hello")"

OUT=$(run_cmd "echo line2 >> /tmp/sh_redir && cat /tmp/sh_redir")
result "stdout append >> appends"          "$(contains "$OUT" "hello")"
result "stdout append >> keeps original"   "$(contains "$OUT" "line2")"

OUT=$(run_cmd_exitcode "cat /tmp/sh_missing_xyz 2>/dev/null ; echo exitcode=\$?")
result "stderr redirect 2>/dev/null hides error" "$(contains "$OUT" "exitcode=1")"

send "rm -f /tmp/sh_redir"
wait_for_prompt

# -------------------------------------------------------------------------
# Pipes
# -------------------------------------------------------------------------
echo "--- pipes ---"

OUT=$(run_cmd "echo hello | cat")
result "simple pipe"                       "$(contains "$OUT" "hello")"

OUT=$(run_cmd "printf 'a\nb\nc\n' | wc -l")
result "pipe to wc -l counts lines"        "$(contains "$OUT" "3")"

OUT=$(run_cmd "echo one two three | wc -w")
result "pipe to wc -w counts words"        "$(contains "$OUT" "3")"

OUT=$(run_cmd "printf 'z\na\nm\n' | cat")
result "multi-word pipe chain"             "$(contains "$OUT" "z")"

# -------------------------------------------------------------------------
# Exit status ($?)
# -------------------------------------------------------------------------
echo "--- exit status ---"

OUT=$(run_cmd_exitcode "true ; echo exitcode=\$?")
result "\$? after true is 0"              "$(contains "$OUT" "exitcode=0")"

OUT=$(run_cmd_exitcode "false ; echo exitcode=\$?")
result "\$? after false is 1"             "$(contains "$OUT" "exitcode=1")"

OUT=$(run_cmd_exitcode "ls /tmp/no_such_sh_xyz ; echo exitcode=\$?")
result "\$? after failing ls is nonzero"  "$(contains "$OUT" "exitcode=1")"

OUT=$(run_cmd_exitcode "echo hi ; echo exitcode=\$?")
result "\$? after echo is 0"              "$(contains "$OUT" "exitcode=0")"

# -------------------------------------------------------------------------
# && and ||
# -------------------------------------------------------------------------
echo "--- && and || ---"

OUT=$(run_cmd "true && echo andpass")
result "&& runs second on success"         "$(contains "$OUT" "andpass")"

OUT=$(run_cmd "false && echo andskip")
result "&& skips second on failure"        "$([ "$(contains "$OUT" "andskip")" = "0" ] && echo 1 || echo 0)"

OUT=$(run_cmd "false || echo orpass")
result "|| runs second on failure"         "$(contains "$OUT" "orpass")"

OUT=$(run_cmd "true || echo orskip")
result "|| skips second on success"        "$([ "$(contains "$OUT" "orskip")" = "0" ] && echo 1 || echo 0)"

OUT=$(run_cmd "true && echo a && echo b")
result "chained &&"                        "$([ "$(contains "$OUT" "a")" = "1" ] && [ "$(contains "$OUT" "b")" = "1" ] && echo 1 || echo 0)"

OUT=$(run_cmd "false || echo x || echo y")
result "|| chain stops at first success"   "$(contains "$OUT" "x")"

# -------------------------------------------------------------------------
# Semicolon sequencing
# -------------------------------------------------------------------------
echo "--- semicolons ---"

OUT=$(run_cmd "echo a ; echo b ; echo c")
result "semicolons run all statements"     "$([ "$(contains "$OUT" "a")" = "1" ] && [ "$(contains "$OUT" "b")" = "1" ] && [ "$(contains "$OUT" "c")" = "1" ] && echo 1 || echo 0)"

OUT=$(run_cmd_exitcode "false ; echo after=\$?")
result "; continues after failure"         "$(contains "$OUT" "after=1")"

# -------------------------------------------------------------------------
# Variable assignment and expansion
# -------------------------------------------------------------------------
echo "--- variables ---"

OUT=$(run_cmd "X=hello ; echo \$X")
result "variable assignment and expansion" "$(contains "$OUT" "hello")"

OUT=$(run_cmd "A=foo ; B=bar ; echo \$A\$B")
result "two variables concatenated"        "$(contains "$OUT" "foobar")"

OUT=$(run_cmd "V=world ; echo hello_\${V}_end")
result "variable in middle of string"      "$(contains "$OUT" "hello_world_end")"

# -------------------------------------------------------------------------
# Quoting
# -------------------------------------------------------------------------
echo "--- quoting ---"

OUT=$(run_cmd 'echo "hello world"')
result "double-quoted string preserved"    "$(contains "$OUT" "hello world")"

OUT=$(run_cmd "echo 'single quoted'")
result "single-quoted string preserved"    "$(contains "$OUT" "single quoted")"

OUT=$(run_cmd 'echo "has  two  spaces"')
result "double-quote preserves spaces"     "$(contains "$OUT" "has  two  spaces")"

# -------------------------------------------------------------------------
# Builtins: cd, pwd
# -------------------------------------------------------------------------
echo "--- cd / pwd ---"

send "cd /tmp"
wait_for_prompt
OUT=$(run_cmd "pwd")
result "cd /tmp then pwd shows /tmp"       "$(contains "$OUT" "/tmp")"

send "cd /"
wait_for_prompt
OUT=$(run_cmd "pwd")
result "cd / then pwd shows /"             "$(contains "$OUT" "/")"

OUT=$(run_cmd_exitcode "cd /no_such_dir_xyz ; echo exitcode=\$?")
result "cd nonexistent exits nonzero"      "$(contains "$OUT" "exitcode=1")"

# -------------------------------------------------------------------------
# Builtins: echo
# -------------------------------------------------------------------------
echo "--- echo builtin ---"

OUT=$(run_cmd "echo hello world")
result "echo builtin basic"                "$(contains "$OUT" "hello world")"

# echo -n leaves no newline so prompt runs onto same line as output.
# Send a newline first to flush, then capture the combined line.
_seq=$(( _seq + 1 ))
_begin="__BEGIN_${_seq}__"
_end="__END_${_seq}__"
send "echo $_begin"
wait_for_prompt
send "echo -n noline"
wait_for_prompt
send "echo $_end"
wait_for_prompt
OUT=$(tmux capture-pane -t "$PANE" -p -S - \
  | awk "/^${_begin}$/{found=1; next} /^${_end}$/{found=0} found")
result "echo -n no trailing newline"       "$(contains "$OUT" "noline")"

# -------------------------------------------------------------------------
# Builtin: which
# -------------------------------------------------------------------------
echo "--- which ---"

OUT=$(run_cmd "which ls")
result "which ls finds /bin/ls"            "$(contains "$OUT" "/bin/ls")"

OUT=$(run_cmd "which sh")
result "which sh finds /bin/sh"            "$(contains "$OUT" "/bin/sh")"

OUT=$(run_cmd_exitcode "which no_such_cmd_xyz ; echo exitcode=\$?")
result "which nonexistent exits nonzero"   "$(contains "$OUT" "exitcode=1")"

# -------------------------------------------------------------------------
# Builtin: export / env
# -------------------------------------------------------------------------
echo "--- export / env ---"

OUT=$(run_cmd "export MYVAR=testval ; env")
result "export makes var visible in env"   "$(contains "$OUT" "MYVAR=testval")"

# -------------------------------------------------------------------------
# Background jobs (&)
# -------------------------------------------------------------------------
echo "--- background jobs ---"

OUT=$(run_cmd "sleep 0 & wait")
result "background job & runs"             "$(equals "$?" "0")"

OUT=$(run_cmd "true & wait && echo waitok")
result "wait waits for background job"     "$(contains "$OUT" "waitok")"

# -------------------------------------------------------------------------
# if / then / else
# -------------------------------------------------------------------------
echo "--- if/then/else ---"

OUT=$(run_cmd "if true ; then echo yes ; fi")
result "if true runs then"                 "$(contains "$OUT" "yes")"

OUT=$(run_cmd "if false ; then echo yes ; else echo no ; fi")
result "if false runs else"                "$(contains "$OUT" "no")"

OUT=$(run_cmd "if test -f /etc/rc ; then echo exists ; fi")
result "if test -f real file"              "$(contains "$OUT" "exists")"

OUT=$(run_cmd "if test -f /no_such_file ; then echo bad ; else echo good ; fi")
result "if test -f missing file"           "$(contains "$OUT" "good")"

# -------------------------------------------------------------------------
# Cleanup
# -------------------------------------------------------------------------
send "rm -f /tmp/sh_redir /tmp/sh_missing_xyz"
wait_for_prompt

print_summary
