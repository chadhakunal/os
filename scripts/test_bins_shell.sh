#!/bin/sh
# Test bin utilities via tmux/shell — covers things that need real shell
# context: pipes, redirects, $?, multi-command sequences.

PANE="${1:-${PANE:-0:0.0}}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/tmux_test_lib.sh"


echo "=== Test: bin utils (shell) ==="

wait_for_prompt

# Ensure /mnt is mounted and /mnt/tmp exists (rc may have done this; ignore errors).
send "mount \"\" /mnt sbfs ; mkdir -p /mnt/tmp"
wait_for_prompt

# -------------------------------------------------------------------------
# cat
# -------------------------------------------------------------------------
echo "--- cat ---"

send "echo hello > /mnt/tmp/tb_cat1 && echo world > /mnt/tmp/tb_cat2"
wait_for_prompt

OUT=$(run_cmd "cat /mnt/tmp/tb_cat1")
result "cat file content"                  "$(contains "$OUT" "hello")"

OUT=$(run_cmd "cat /mnt/tmp/tb_cat1 /mnt/tmp/tb_cat2")
result "cat two files concatenates"        "$([ "$(contains "$OUT" "hello")" = "1" ] && [ "$(contains "$OUT" "world")" = "1" ] && echo 1 || echo 0)"

OUT=$(run_cmd_exitcode "cat /mnt/tmp/tb_cat1 /mnt/tmp/no_such_cat ; echo exitcode=\$?")
result "cat missing file exits nonzero"    "$(contains "$OUT" "exitcode=1")"

OUT=$(run_cmd "echo piped | cat")
result "cat reads from pipe"               "$(contains "$OUT" "piped")"

OUT=$(run_cmd "cat /mnt/tmp/tb_cat1 | cat | cat")
result "cat chained three times"           "$(contains "$OUT" "hello")"

send "rm -f /mnt/tmp/tb_cat1 /mnt/tmp/tb_cat2"
wait_for_prompt

# -------------------------------------------------------------------------
# echo
# -------------------------------------------------------------------------
echo "--- echo ---"

OUT=$(run_cmd "echo foo bar baz")
result "echo multiple args"                "$(contains "$OUT" "foo bar baz")"

# echo -n: no trailing newline so output merges with prompt on same line.
# Use raw capture (no prompt-line filter) so the merged line is kept.
_seq=$(( _seq + 1 ))
_b="__BEGIN_${_seq}__" ; _e="__END_${_seq}__"
send "echo $_b" ; wait_for_prompt
send "echo -n noline" ; wait_for_prompt
send "echo $_e" ; wait_for_prompt
_raw=$(tmux capture-pane -t "$PANE" -p -S - \
  | awk "/^${_b}$/{f=1;next} /^${_e}$/{f=0} f")
result "echo -n no newline"                "$(contains "$_raw" "noline")"

# echo with no args: blank line is stripped by run_cmd — use run_cmd_raw.
OUT=$(run_cmd_raw "echo")
BLANK=$(echo "$OUT" | grep -c '^[[:space:]]*$' || true)
result "echo no args prints blank line"    "$([ "$BLANK" -ge 1 ] && echo 1 || echo 0)"

# -------------------------------------------------------------------------
# printf
# -------------------------------------------------------------------------
echo "--- printf ---"

OUT=$(run_cmd "printf '%s\n' hello")
result "printf %s"                         "$(contains "$OUT" "hello")"

OUT=$(run_cmd "printf '%d\n' 42")
result "printf %d"                         "$(contains "$OUT" "42")"

OUT=$(run_cmd "printf '%s=%d\n' x 5")
result "printf multiple specifiers"        "$(contains "$OUT" "x=5")"

OUT=$(run_cmd "printf '%d\n' 1 2 3")
result "printf format reuse"               "$([ "$(contains "$OUT" "1")" = "1" ] && [ "$(contains "$OUT" "2")" = "1" ] && [ "$(contains "$OUT" "3")" = "1" ] && echo 1 || echo 0)"

OUT=$(run_cmd "printf 'hello\n' | cat")
result "printf output pipeable"            "$(contains "$OUT" "hello")"

OUT=$(run_cmd "printf 'data\n' > /mnt/tmp/tb_printf_out && cat /mnt/tmp/tb_printf_out")
result "printf redirect to file"           "$(contains "$OUT" "data")"
send "rm -f /mnt/tmp/tb_printf_out"
wait_for_prompt

# -------------------------------------------------------------------------
# wc
# -------------------------------------------------------------------------
echo "--- wc ---"

send "printf 'a\nb\nc\n' > /mnt/tmp/tb_wc"
wait_for_prompt

OUT=$(run_cmd "wc -l /mnt/tmp/tb_wc")
result "wc -l counts lines"               "$(contains "$OUT" "3")"

OUT=$(run_cmd "wc -w /mnt/tmp/tb_wc")
result "wc -w counts words"               "$(contains "$OUT" "3")"

OUT=$(run_cmd "wc -c /mnt/tmp/tb_wc")
result "wc -c counts bytes"               "$(contains "$OUT" "6")"

OUT=$(run_cmd "cat /mnt/tmp/tb_wc | wc -l")
result "wc -l via pipe"                   "$(contains "$OUT" "3")"

OUT=$(run_cmd "echo one two three | wc -w")
result "echo | wc -w"                     "$(contains "$OUT" "3")"

send "rm -f /mnt/tmp/tb_wc"
wait_for_prompt

# -------------------------------------------------------------------------
# head / tail
# -------------------------------------------------------------------------
echo "--- head / tail ---"

send "printf 'l1\nl2\nl3\nl4\nl5\n' > /mnt/tmp/tb_ht"
wait_for_prompt

OUT=$(run_cmd "head -n 2 /mnt/tmp/tb_ht")
result "head -n 2 first 2 lines"          "$([ "$(contains "$OUT" "l1")" = "1" ] && [ "$(contains "$OUT" "l2")" = "1" ] && [ "$(contains "$OUT" "l3")" = "0" ] && echo 1 || echo 0)"

OUT=$(run_cmd "tail -n 2 /mnt/tmp/tb_ht")
result "tail -n 2 last 2 lines"           "$([ "$(contains "$OUT" "l4")" = "1" ] && [ "$(contains "$OUT" "l5")" = "1" ] && [ "$(contains "$OUT" "l3")" = "0" ] && echo 1 || echo 0)"

OUT=$(run_cmd "cat /mnt/tmp/tb_ht | head -n 1")
result "head via pipe"                    "$(contains "$OUT" "l1")"

OUT=$(run_cmd "cat /mnt/tmp/tb_ht | tail -n 1")
result "tail via pipe"                    "$(contains "$OUT" "l5")"

send "rm -f /mnt/tmp/tb_ht"
wait_for_prompt

# -------------------------------------------------------------------------
# grep (if present in guest)
# -------------------------------------------------------------------------
GREP_CHECK=$(run_cmd "which grep")
if [ "$(contains "$GREP_CHECK" "/bin/grep")" = "1" ]; then
  echo "--- grep ---"
  send "printf 'apple\nbanana\ncherry\n' > /mnt/tmp/tb_grep"
  wait_for_prompt
  OUT=$(run_cmd "grep banana /mnt/tmp/tb_grep")
  result "grep finds matching line"         "$(contains "$OUT" "banana")"
  OUT=$(run_cmd "grep apple /mnt/tmp/tb_grep")
  result "grep does not print non-match"    "$([ "$(contains "$OUT" "banana")" = "0" ] && echo 1 || echo 0)"
  OUT=$(run_cmd_exitcode "grep nomatch /mnt/tmp/tb_grep ; echo exitcode=\$?")
  result "grep no match exits 1"            "$(contains "$OUT" "exitcode=1")"
  OUT=$(run_cmd "cat /mnt/tmp/tb_grep | grep cherry")
  result "grep via pipe"                    "$(contains "$OUT" "cherry")"
  send "rm -f /mnt/tmp/tb_grep"
  wait_for_prompt
else
  echo "--- grep not found, skipping ---"
fi

# -------------------------------------------------------------------------
# touch / mkdir / rmdir / rm
# -------------------------------------------------------------------------
echo "--- touch / mkdir / rmdir / rm ---"

OUT=$(run_cmd_exitcode "touch /mnt/tmp/tb_touch_sh && echo ok=\$?")
result "touch creates file"                "$(contains "$OUT" "ok=0")"

OUT=$(run_cmd_exitcode "test -f /mnt/tmp/tb_touch_sh && echo exists")
result "touched file exists"               "$(contains "$OUT" "exists")"

OUT=$(run_cmd_exitcode "rm /mnt/tmp/tb_touch_sh && echo ok=\$?")
result "rm removes file"                   "$(contains "$OUT" "ok=0")"

OUT=$(run_cmd_exitcode "test -f /mnt/tmp/tb_touch_sh ; echo exitcode=\$?")
result "file gone after rm"                "$(contains "$OUT" "exitcode=1")"

OUT=$(run_cmd_exitcode "mkdir /mnt/tmp/tb_mkdir_sh && echo ok=\$?")
result "mkdir creates dir"                 "$(contains "$OUT" "ok=0")"

OUT=$(run_cmd_exitcode "test -d /mnt/tmp/tb_mkdir_sh && echo isdir")
result "mkdir: is directory"               "$(contains "$OUT" "isdir")"

OUT=$(run_cmd_exitcode "rmdir /mnt/tmp/tb_mkdir_sh && echo ok=\$?")
result "rmdir removes empty dir"           "$(contains "$OUT" "ok=0")"

OUT=$(run_cmd_exitcode "rm -rf /mnt/tmp/no_such_rmrf_sh ; echo exitcode=\$?")
result "rm -rf nonexistent exits 0"        "$(contains "$OUT" "exitcode=0")"

send "mkdir -p /mnt/tmp/tb_rmrf_sh/a/b && touch /mnt/tmp/tb_rmrf_sh/a/b/f"
wait_for_prompt
OUT=$(run_cmd_exitcode "rm -rf /mnt/tmp/tb_rmrf_sh && echo ok=\$?")
result "rm -rf nested tree"                "$(contains "$OUT" "ok=0")"
OUT=$(run_cmd_exitcode "test -d /mnt/tmp/tb_rmrf_sh ; echo exitcode=\$?")
result "rm -rf: tree gone"                 "$(contains "$OUT" "exitcode=1")"

# -------------------------------------------------------------------------
# cp / mv
# -------------------------------------------------------------------------
echo "--- cp / mv ---"

send "echo cpdata > /mnt/tmp/tb_cp_src_sh"
wait_for_prompt

OUT=$(run_cmd_exitcode "cp /mnt/tmp/tb_cp_src_sh /mnt/tmp/tb_cp_dst_sh && echo ok=\$?")
result "cp exits 0"                        "$(contains "$OUT" "ok=0")"

OUT=$(run_cmd "cat /mnt/tmp/tb_cp_dst_sh")
result "cp content preserved"              "$(contains "$OUT" "cpdata")"

OUT=$(run_cmd_exitcode "cp /mnt/tmp/no_such_cp ; echo exitcode=\$?")
result "cp missing src exits nonzero"      "$(contains "$OUT" "exitcode=1")"

send "rm -f /mnt/tmp/tb_cp_src_sh /mnt/tmp/tb_cp_dst_sh"
wait_for_prompt

send "echo mvdata > /mnt/tmp/tb_mv_src_sh"
wait_for_prompt

OUT=$(run_cmd_exitcode "mv /mnt/tmp/tb_mv_src_sh /mnt/tmp/tb_mv_dst_sh && echo ok=\$?")
result "mv exits 0"                        "$(contains "$OUT" "ok=0")"

OUT=$(run_cmd_exitcode "test -f /mnt/tmp/tb_mv_src_sh ; echo exitcode=\$?")
result "mv: src gone"                      "$(contains "$OUT" "exitcode=1")"

OUT=$(run_cmd "cat /mnt/tmp/tb_mv_dst_sh")
result "mv: content at dst"               "$(contains "$OUT" "mvdata")"

send "rm -f /mnt/tmp/tb_mv_dst_sh"
wait_for_prompt

# -------------------------------------------------------------------------
# chmod
# -------------------------------------------------------------------------
echo "--- chmod ---"

send "touch /mnt/tmp/tb_chmod_sh"
wait_for_prompt

OUT=$(run_cmd_exitcode "chmod 755 /mnt/tmp/tb_chmod_sh && echo ok=\$?")
result "chmod exits 0"                     "$(contains "$OUT" "ok=0")"

OUT=$(run_cmd "ls -l /mnt/tmp/tb_chmod_sh")
result "chmod 755: exec bits in ls -l"     "$(contains "$OUT" "x")"

OUT=$(run_cmd_exitcode "chmod 644 /mnt/tmp/tb_chmod_sh && echo ok=\$?")
result "chmod 644 exits 0"                 "$(contains "$OUT" "ok=0")"

OUT=$(run_cmd "ls -l /mnt/tmp/tb_chmod_sh")
result "chmod 644: no exec in ls -l"       "$([ "$(matches "$OUT" '^-rw')" = "1" ] && echo 1 || echo 0)"

send "rm -f /mnt/tmp/tb_chmod_sh"
wait_for_prompt

# -------------------------------------------------------------------------
# env
# -------------------------------------------------------------------------
echo "--- env ---"

OUT=$(run_cmd "env")
result "env prints environment"            "$(contains "$OUT" "PATH=")"

OUT=$(run_cmd "MYVAR=hello env")
result "env MYVAR=hello shows var"         "$(contains "$OUT" "MYVAR=hello")"

OUT=$(run_cmd "env -i PATH=/bin env")
result "env -i PATH=/bin: only PATH"       "$([ "$(contains "$OUT" "PATH=/bin")" = "1" ] && echo 1 || echo 0)"

# -------------------------------------------------------------------------
# sleep
# -------------------------------------------------------------------------
echo "--- sleep ---"

OUT=$(run_cmd_exitcode "sleep 0 && echo ok=\$?")
result "sleep 0 exits 0"                   "$(contains "$OUT" "ok=0")"

OUT=$(run_cmd_exitcode "sleep ; echo exitcode=\$?")
result "sleep no args exits nonzero"       "$(contains "$OUT" "exitcode=1")"

# -------------------------------------------------------------------------
# test binary
# -------------------------------------------------------------------------
echo "--- test ---"

OUT=$(run_cmd_exitcode "test -f /etc/rc && echo ok")
result "test -f existing file"             "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -d /bin && echo ok")
result "test -d directory"                 "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test -f /no_such ; echo exitcode=\$?")
result "test -f missing exits 1"           "$(contains "$OUT" "exitcode=1")"

OUT=$(run_cmd_exitcode "test hello = hello && echo ok")
result "test string equality"              "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "test 3 -gt 2 && echo ok")
result "test integer -gt"                  "$(contains "$OUT" "ok")"

# -------------------------------------------------------------------------
# true / false
# -------------------------------------------------------------------------
echo "--- true / false ---"

OUT=$(run_cmd_exitcode "true && echo ok")
result "true exits 0"                      "$(contains "$OUT" "ok")"

OUT=$(run_cmd_exitcode "false ; echo exitcode=\$?")
result "false exits nonzero"               "$(contains "$OUT" "exitcode=1")"

# -------------------------------------------------------------------------
# pwd
# -------------------------------------------------------------------------
echo "--- pwd ---"

OUT=$(run_cmd "pwd")
result "pwd prints path starting with /"   "$(matches "$OUT" '^/')"

# -------------------------------------------------------------------------
# ps
# -------------------------------------------------------------------------
echo "--- ps ---"

OUT=$(run_cmd "ps")
result "ps exits and has output"           "$(contains "$OUT" "PID")"

# -------------------------------------------------------------------------
# df
# -------------------------------------------------------------------------
echo "--- df ---"

OUT=$(run_cmd "df")
result "df shows filesystem header"        "$(contains "$OUT" "Filesystem")"

OUT=$(run_cmd "df /mnt/tmp")
result "df /tmp exits ok"                  "$(contains "$OUT" "Use%")"

# -------------------------------------------------------------------------
# Pipe chains
# -------------------------------------------------------------------------
echo "--- pipe chains ---"

send "printf 'dog\ncat\nbird\ncat\n' > /mnt/tmp/tb_pipe_sh"
wait_for_prompt

OUT=$(run_cmd "cat /mnt/tmp/tb_pipe_sh | wc -l")
result "cat | wc -l"                       "$(contains "$OUT" "4")"

OUT=$(run_cmd "head -n 2 /mnt/tmp/tb_pipe_sh | wc -l")
result "head | wc -l"                      "$(contains "$OUT" "2")"

OUT=$(run_cmd "tail -n 1 /mnt/tmp/tb_pipe_sh | cat")
result "tail | cat"                        "$(contains "$OUT" "cat")"

send "rm -f /mnt/tmp/tb_pipe_sh"
wait_for_prompt

# -------------------------------------------------------------------------
# Redirection combined with commands
# -------------------------------------------------------------------------
echo "--- redirect chains ---"

OUT=$(run_cmd "echo lineA > /mnt/tmp/tb_redir_sh && echo lineB >> /mnt/tmp/tb_redir_sh && cat /mnt/tmp/tb_redir_sh")
result "> then >> appends correctly"       "$([ "$(contains "$OUT" "lineA")" = "1" ] && [ "$(contains "$OUT" "lineB")" = "1" ] && echo 1 || echo 0)"

OUT=$(run_cmd "wc -l < /mnt/tmp/tb_redir_sh")
result "wc -l with stdin redirect"        "$(contains "$OUT" "2")"

send "rm -f /mnt/tmp/tb_redir_sh"
wait_for_prompt

print_summary
