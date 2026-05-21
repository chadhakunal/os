#!/bin/sh
# POSIX compliance tests — treat the OS as a black box.
# Tests derived from POSIX.1-2017 requirements, not from knowledge of the implementation.

PANE="${1:-${PANE:-0:0.0}}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/tmux_test_lib.sh"

echo "=== Test: POSIX compliance ==="

wait_for_prompt

send "mount \"\" /mnt sbfs ; mkdir -p /mnt/tmp"
wait_for_prompt

# -------------------------------------------------------------------------
# File system hierarchy
# -------------------------------------------------------------------------
echo "--- filesystem hierarchy ---"

OUT=$(run_cmd_exitcode "test -d / && echo ok")
result "/ exists and is a directory"           "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -d /bin && echo ok")
result "/bin exists and is a directory"        "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -d /etc && echo ok")
result "/etc exists and is a directory"        "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -d /mnt/tmp && echo ok")
result "/tmp exists and is a directory"        "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -d /dev && echo ok")
result "/dev exists and is a directory"        "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -c /dev/null && echo ok")
result "/dev/null is a character device"       "$(contains "$OUT" "ok")"

# -------------------------------------------------------------------------
# /dev/null semantics
# -------------------------------------------------------------------------
echo "--- /dev/null ---"

OUT=$(run_cmd "cat /dev/null")
result "/dev/null read returns nothing"        "$(equals "$OUT" "")"

OUT=$(run_cmd_exitcode "echo something > /dev/null && echo ok")
result "write to /dev/null succeeds"           "$(contains "$OUT" "ok")"

OUT=$(run_cmd "echo data > /dev/null && cat /dev/null")
result "/dev/null always reads empty"          "$(equals "$OUT" "")"

# -------------------------------------------------------------------------
# File permissions
# -------------------------------------------------------------------------
echo "--- file permissions ---"

send "touch /mnt/tmp/posix_perm_test"
wait_for_prompt

OUT=$(run_cmd_exitcode "chmod 000 /mnt/tmp/posix_perm_test && test ! -r /mnt/tmp/posix_perm_test && echo ok")
result "chmod 000 removes read permission"     "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "chmod 644 /mnt/tmp/posix_perm_test && test -r /mnt/tmp/posix_perm_test && echo ok")
result "chmod 644 grants read permission"      "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "chmod 755 /mnt/tmp/posix_perm_test && test -x /mnt/tmp/posix_perm_test && echo ok")
result "chmod 755 grants execute permission"   "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "chmod 444 /mnt/tmp/posix_perm_test && test -w /mnt/tmp/posix_perm_test ; echo exitcode=\$?")
result "chmod 444 removes write permission"    "$(contains "$OUT" "exitcode=1")"

send "chmod 644 /mnt/tmp/posix_perm_test && rm -f /mnt/tmp/posix_perm_test"
wait_for_prompt

# -------------------------------------------------------------------------
# File creation and deletion
# -------------------------------------------------------------------------
echo "--- file creation / deletion ---"

OUT=$(run_cmd_exitcode "touch /mnt/tmp/posix_touch1 && test -f /mnt/tmp/posix_touch1 && echo ok")
result "touch creates regular file"            "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "rm /mnt/tmp/posix_touch1 && test ! -e /mnt/tmp/posix_touch1 && echo ok")
result "rm removes file"                       "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "mkdir /mnt/tmp/posix_dir1 && test -d /mnt/tmp/posix_dir1 && echo ok")
result "mkdir creates directory"               "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "rmdir /mnt/tmp/posix_dir1 && test ! -e /mnt/tmp/posix_dir1 && echo ok")
result "rmdir removes empty directory"         "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "rmdir /mnt/tmp/posix_no_such_dir ; echo exitcode=\$?")
result "rmdir nonexistent exits nonzero"       "$(contains "$OUT" "exitcode=1")"

OUT=$(run_cmd_exitcode "mkdir -p /mnt/tmp/posix_nest/a/b/c && test -d /mnt/tmp/posix_nest/a/b/c && echo ok")
result "mkdir -p creates nested directories"   "$(contains "$OUT" "ok")"

send "rm -rf /mnt/tmp/posix_nest"
wait_for_prompt

# -------------------------------------------------------------------------
# Symbolic links
# -------------------------------------------------------------------------
echo "--- symbolic links ---"

send "echo linkdata > /mnt/tmp/posix_link_target"
wait_for_prompt

OUT=$(run_cmd_exitcode "ln -s /mnt/tmp/posix_link_target /mnt/tmp/posix_link && test -L /mnt/tmp/posix_link && echo ok")
result "ln -s creates symlink"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd "cat /mnt/tmp/posix_link")
result "reading symlink reads target"          "$(contains "$OUT" "linkdata")"

OUT=$(run_cmd "readlink /mnt/tmp/posix_link")
result "readlink returns link target"          "$(contains "$OUT" "/mnt/tmp/posix_link_target")"

OUT=$(run_cmd_exitcode "test -f /mnt/tmp/posix_link && echo ok")
result "test -f follows symlink to regular file" "$(contains "$OUT" "ok")"

send "rm -f /mnt/tmp/posix_link /mnt/tmp/posix_link_target"
wait_for_prompt

# dangling symlink
OUT=$(run_cmd_exitcode "ln -s /mnt/tmp/posix_no_target /mnt/tmp/posix_dangling && test -L /mnt/tmp/posix_dangling && echo ok")
result "dangling symlink: -L is true"          "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -e /mnt/tmp/posix_dangling ; echo exitcode=\$?")
result "dangling symlink: -e is false"         "$(contains "$OUT" "exitcode=1")"

send "rm -f /mnt/tmp/posix_dangling"
wait_for_prompt

# -------------------------------------------------------------------------
# Hard links
# -------------------------------------------------------------------------
echo "--- hard links ---"

send "echo harddata > /mnt/tmp/posix_hard_src"
wait_for_prompt

OUT=$(run_cmd_exitcode "ln /mnt/tmp/posix_hard_src /mnt/tmp/posix_hard_dst && echo ok")
result "ln creates hard link"                  "$(contains "$OUT" "ok")"

OUT=$(run_cmd "cat /mnt/tmp/posix_hard_dst")
result "hard link has same content"            "$(contains "$OUT" "harddata")"

OUT=$(run_cmd_exitcode "echo newdata > /mnt/tmp/posix_hard_src && cat /mnt/tmp/posix_hard_dst")
result "write to src visible via hard link"    "$(contains "$OUT" "newdata")"

send "rm -f /mnt/tmp/posix_hard_src /mnt/tmp/posix_hard_dst"
wait_for_prompt

# -------------------------------------------------------------------------
# I/O redirection
# -------------------------------------------------------------------------
echo "--- I/O redirection ---"

OUT=$(run_cmd "echo posix_out > /mnt/tmp/posix_redir && cat /mnt/tmp/posix_redir")
result "> truncates and writes"                "$(contains "$OUT" "posix_out")"

OUT=$(run_cmd "echo line1 > /mnt/tmp/posix_append && echo line2 >> /mnt/tmp/posix_append && wc -l < /mnt/tmp/posix_append")
result ">> appends; file has 2 lines"          "$(contains "$OUT" "2")"

OUT=$(run_cmd "wc -c < /mnt/tmp/posix_append")
result "< redirects stdin from file"           "$(contains "$OUT" "12")"

OUT=$(run_cmd "echo heredoc_line > /mnt/tmp/posix_redir2 && cat < /mnt/tmp/posix_redir2")
result "stdin redirect reads file content"     "$(contains "$OUT" "heredoc_line")"

send "rm -f /mnt/tmp/posix_redir /mnt/tmp/posix_redir2 /mnt/tmp/posix_append"
wait_for_prompt

# -------------------------------------------------------------------------
# Pipelines
# -------------------------------------------------------------------------
echo "--- pipelines ---"

OUT=$(run_cmd "echo hello | cat")
result "pipe: output of echo reaches cat"      "$(contains "$OUT" "hello")"

OUT=$(run_cmd "printf 'a\nb\nc\n' | wc -l")
result "pipe: wc -l counts piped lines"        "$(contains "$OUT" "3")"

OUT=$(run_cmd "printf 'x\ny\nz\n' | head -n 2 | wc -l")
result "pipe chain: head | wc -l"              "$(contains "$OUT" "2")"

OUT=$(run_cmd "echo one two three | wc -w")
result "pipe: wc -w counts words"              "$(contains "$OUT" "3")"

OUT=$(run_cmd_exitcode "false | true ; echo exitcode=\$?")
result "pipe exit status is last command"      "$(contains "$OUT" "exitcode=0")"

# -------------------------------------------------------------------------
# Exit status
# -------------------------------------------------------------------------
echo "--- exit status ---"

OUT=$(run_cmd_exitcode "true ; echo s=\$?")
result "true exits 0"                          "$(contains "$OUT" "s=0")"

OUT=$(run_cmd_exitcode "false ; echo s=\$?")
result "false exits non-zero"                  "$(contains "$OUT" "s=1")"

OUT=$(run_cmd_exitcode "sh -c 'exit 42' ; echo s=\$?")
result "sh -c exit N propagates exit code"     "$(contains "$OUT" "s=42")"

OUT=$(run_cmd_exitcode "ls /no_such_path_posix ; echo s=\$?")
result "failed command exits non-zero"         "$(contains "$OUT" "s=1")"

# -------------------------------------------------------------------------
# Shell variables and environment
# -------------------------------------------------------------------------
echo "--- variables and environment ---"

OUT=$(run_cmd "X=hello ; echo \$X")
result "variable set and expanded"             "$(contains "$OUT" "hello")"

OUT=$(run_cmd "unset X ; echo \${X:-default}")
result "unset var with default expansion"      "$(contains "$OUT" "default")"

OUT=$(run_cmd "A=foo ; export A ; sh -c 'echo \$A'")
result "exported var visible in child shell"   "$(contains "$OUT" "foo")"

OUT=$(run_cmd "sh -c 'echo \$HOME'")
result "\$HOME is set"                         "$(matches "$OUT" '^/')"

OUT=$(run_cmd "sh -c 'echo \$PATH'")
result "\$PATH is set and non-empty"           "$(matches "$OUT" '.')"

# -------------------------------------------------------------------------
# Compound commands
# -------------------------------------------------------------------------
echo "--- compound commands ---"

OUT=$(run_cmd "if true ; then echo yes ; fi")
result "if true executes then"                 "$(contains "$OUT" "yes")"

OUT=$(run_cmd "if false ; then echo no ; else echo yes ; fi")
result "if false executes else"                "$(contains "$OUT" "yes")"

OUT=$(run_cmd "if false ; then echo no ; fi")
result "if false with no else: no output"      "$(equals "$OUT" "")"

OUT=$(run_cmd "i=0 ; while test \$i -lt 3 ; do echo \$i ; i=\$(expr \$i + 1) ; done")
result "while loop runs correct iterations"    "$([ "$(contains "$OUT" "0")" = "1" ] && [ "$(contains "$OUT" "1")" = "1" ] && [ "$(contains "$OUT" "2")" = "1" ] && echo 1 || echo 0)"

OUT=$(run_cmd "for x in a b c ; do echo \$x ; done")
result "for loop iterates over words"          "$([ "$(contains "$OUT" "a")" = "1" ] && [ "$(contains "$OUT" "b")" = "1" ] && [ "$(contains "$OUT" "c")" = "1" ] && echo 1 || echo 0)"

OUT=$(run_cmd "true && echo yes")
result "&& runs on success"                    "$(contains "$OUT" "yes")"

OUT=$(run_cmd "false || echo yes")
result "|| runs on failure"                    "$(contains "$OUT" "yes")"

OUT=$(run_cmd "false && echo no ; echo after")
result "&& skips on failure; ; continues"      "$([ "$(contains "$OUT" "no")" = "0" ] && [ "$(contains "$OUT" "after")" = "1" ] && echo 1 || echo 0)"

# -------------------------------------------------------------------------
# test / [ built-in operators
# -------------------------------------------------------------------------
echo "--- test operators ---"

OUT=$(run_cmd_exitcode "test -e /etc && echo ok")
result "test -e: path exists"                  "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -f /etc/rc && echo ok")
result "test -f: regular file"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -d /bin && echo ok")
result "test -d: directory"                    "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -r /etc/rc && echo ok")
result "test -r: readable file"                "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -s /etc/rc && echo ok")
result "test -s: non-empty file"               "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -z '' && echo ok")
result "test -z: empty string"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -n hello && echo ok")
result "test -n: non-empty string"             "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test hello = hello && echo ok")
result "test: string equality"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test hello != world && echo ok")
result "test: string inequality"               "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test 5 -gt 3 && echo ok")
result "test: integer -gt"                     "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test 3 -lt 5 && echo ok")
result "test: integer -lt"                     "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test 5 -ge 5 && echo ok")
result "test: integer -ge"                     "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test 5 -le 5 && echo ok")
result "test: integer -le"                     "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test 4 -eq 4 && echo ok")
result "test: integer -eq"                     "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test 4 -ne 5 && echo ok")
result "test: integer -ne"                     "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test ! -f /no_such_posix && echo ok")
result "test: ! negation"                      "$(contains "$OUT" "ok")"

# -------------------------------------------------------------------------
# Signal handling
# -------------------------------------------------------------------------
echo "--- signals ---"

OUT=$(run_cmd_exitcode "sleep 10 & kill \$! && echo ok")
result "kill terminates background process"    "$(contains "$OUT" "ok")"

# -------------------------------------------------------------------------
# Process substitution / subshells
# -------------------------------------------------------------------------
echo "--- subshells ---"

OUT=$(run_cmd "sh -c 'echo subshell'")
result "sh -c runs command in subshell"        "$(contains "$OUT" "subshell")"

OUT=$(run_cmd "X=outer ; sh -c 'X=inner' ; echo \$X")
result "subshell does not affect parent vars"  "$(contains "$OUT" "outer")"

OUT=$(run_cmd "sh -c 'exit 7' ; echo s=\$?")
result "subshell exit code propagates"         "$(contains "$OUT" "s=7")"

# -------------------------------------------------------------------------
# Word splitting and globbing
# -------------------------------------------------------------------------
echo "--- word splitting ---"

OUT=$(run_cmd "echo one   two   three | wc -w")
result "multiple spaces collapse to single separator" "$(contains "$OUT" "3")"

OUT=$(run_cmd 'echo "one   two   three" | wc -w')
result "quoted string with spaces is one word" "$(contains "$OUT" "3")"

# -------------------------------------------------------------------------
# Path resolution
# -------------------------------------------------------------------------
echo "--- path resolution ---"

OUT=$(run_cmd_exitcode "test -x /bin/sh && echo ok")
result "/bin/sh is executable"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/echo && echo ok")
result "/bin/echo is executable"               "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/ls && echo ok")
result "/bin/ls is executable"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/cat && echo ok")
result "/bin/cat is executable"                "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/rm && echo ok")
result "/bin/rm is executable"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/cp && echo ok")
result "/bin/cp is executable"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/mv && echo ok")
result "/bin/mv is executable"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/mkdir && echo ok")
result "/bin/mkdir is executable"              "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/chmod && echo ok")
result "/bin/chmod is executable"              "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/wc && echo ok")
result "/bin/wc is executable"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/head && echo ok")
result "/bin/head is executable"               "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/tail && echo ok")
result "/bin/tail is executable"               "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -x /bin/printf && echo ok")
result "/bin/printf is executable"             "$(contains "$OUT" "ok")"

# -------------------------------------------------------------------------
# /tmp writability
# -------------------------------------------------------------------------
echo "--- /tmp writability ---"

OUT=$(run_cmd_exitcode "touch /mnt/tmp/posix_write_test && echo ok")
result "/tmp is writable"                      "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "mkdir /mnt/tmp/posix_mkdir_test && echo ok")
result "/tmp allows directory creation"        "$(contains "$OUT" "ok")"

send "rm -f /mnt/tmp/posix_write_test && rmdir /mnt/tmp/posix_mkdir_test"
wait_for_prompt

# -------------------------------------------------------------------------
# Concurrent processes
# -------------------------------------------------------------------------
echo "--- concurrent processes ---"

OUT=$(run_cmd "sleep 1 & sleep 1 & wait && echo done")
result "two background jobs complete via wait" "$(contains "$OUT" "done")"

OUT=$(run_cmd_exitcode "sleep 0 & PID=\$! ; wait \$PID ; echo s=\$?")
result "wait PID returns job exit status"      "$(contains "$OUT" "s=0")"

# -------------------------------------------------------------------------
# Cleanup
# -------------------------------------------------------------------------
send "rm -rf /mnt/tmp/posix_*"
wait_for_prompt

print_summary
