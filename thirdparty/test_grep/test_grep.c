#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

extern char **environ;

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
  if (ok) { printf("  [PASS] %s\n", name); passed++; }
  else     { printf("  [FAIL] %s\n", name); failed++; }
}

static int child_ok(int status) {
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int child_exit(int status) {
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Run argv, redirect stdin from in_fd (or /dev/null if -1), capture stdout
 * into buf, return waitpid status. */
static int run_capture_stdin(char *const argv[], int in_fd, char *buf, size_t bufsz) {
  int out_fds[2];
  pipe(out_fds);
  pid_t pid = fork();
  if (pid == 0) {
    if (in_fd >= 0) {
      dup2(in_fd, 0);
      close(in_fd);
    }
    close(out_fds[0]);
    dup2(out_fds[1], 1);
    close(out_fds[1]);
    execve(argv[0], argv, environ);
    _exit(127);
  }
  close(out_fds[1]);
  size_t total = 0;
  ssize_t n;
  while (total < bufsz - 1 && (n = read(out_fds[0], buf + total, bufsz - 1 - total)) > 0)
    total += n;
  buf[total] = '\0';
  close(out_fds[0]);
  int status;
  waitpid(pid, &status, 0);
  return status;
}

static int run_capture(char *const argv[], char *buf, size_t bufsz) {
  return run_capture_stdin(argv, -1, buf, bufsz);
}

static int run_exit(char *const argv[]) {
  char buf[4];
  return run_capture(argv, buf, sizeof(buf));
}

static int write_file(const char *path, const char *content) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return -1;
  size_t len = strlen(content);
  ssize_t n = write(fd, content, len);
  close(fd);
  return (n == (ssize_t)len) ? 0 : -1;
}

/* Feed input_str to grep's stdin via a pipe, capture stdout. */
static int grep_stdin(char *const argv[], const char *input, char *buf, size_t bufsz) {
  int in_fds[2], out_fds[2];
  pipe(in_fds);
  pipe(out_fds);
  pid_t pid = fork();
  if (pid == 0) {
    close(in_fds[1]); close(out_fds[0]);
    dup2(in_fds[0], 0); close(in_fds[0]);
    dup2(out_fds[1], 1); close(out_fds[1]);
    execve(argv[0], argv, environ);
    _exit(127);
  }
  close(in_fds[0]); close(out_fds[1]);
  size_t len = strlen(input);
  write(in_fds[1], input, len);
  close(in_fds[1]);
  size_t total = 0;
  ssize_t n;
  while (total < bufsz - 1 && (n = read(out_fds[0], buf + total, bufsz - 1 - total)) > 0)
    total += n;
  buf[total] = '\0';
  close(out_fds[0]);
  int status;
  waitpid(pid, &status, 0);
  return status;
}

/* -----------------------------------------------------------------------
 * Basic matching from stdin
 * -------------------------------------------------------------------- */
static void test_basic_match(void) {
  printf("Test: basic match (stdin)\n");
  char buf[256];

  char *a1[] = { "/bin/grep", "foo", NULL };
  int st = grep_stdin(a1, "foo\nbar\nbaz\n", buf, sizeof(buf));
  result("grep matches line with pattern", child_ok(st));
  result("grep stdout is matching line", strcmp(buf, "foo\n") == 0);

  char *a2[] = { "/bin/grep", "bar", NULL };
  grep_stdin(a2, "foo\nbar\nbaz\n", buf, sizeof(buf));
  result("grep matches middle line", strcmp(buf, "bar\n") == 0);

  /* no match → exit 1, no output */
  char *a3[] = { "/bin/grep", "xyz", NULL };
  st = grep_stdin(a3, "foo\nbar\n", buf, sizeof(buf));
  result("grep no match exits 1", child_exit(st) == 1);
  result("grep no match: no output", strcmp(buf, "") == 0);
}

/* -----------------------------------------------------------------------
 * Matching from a file
 * -------------------------------------------------------------------- */
static void test_file_match(void) {
  printf("Test: file match\n");
  char buf[256];

  write_file("/tmp/tg_file", "apple\nbanana\ncherry\n");

  char *a1[] = { "/bin/grep", "banana", "/tmp/tg_file", NULL };
  int st = run_capture(a1, buf, sizeof(buf));
  result("grep file: exits 0 on match", child_ok(st));
  result("grep file: correct line", strcmp(buf, "banana\n") == 0);

  char *a2[] = { "/bin/grep", "mango", "/tmp/tg_file", NULL };
  st = run_exit(a2);
  result("grep file: exits 1 on no match", child_exit(st) == 1);

  /* missing file — exits 2 (usage/error) or non-zero */
  char *a3[] = { "/bin/grep", "foo", "/tmp/no_such_file_tg_xyz", NULL };
  st = run_exit(a3);
  result("grep missing file: exits non-zero", WIFEXITED(st) && WEXITSTATUS(st) != 0);

  unlink("/tmp/tg_file");
}

/* -----------------------------------------------------------------------
 * -n flag: line numbers
 * -------------------------------------------------------------------- */
static void test_flag_n(void) {
  printf("Test: -n (line numbers)\n");
  char buf[256];

  char *a1[] = { "/bin/grep", "-n", "bar", NULL };
  grep_stdin(a1, "foo\nbar\nbaz\n", buf, sizeof(buf));
  result("grep -n prefixes line number", strcmp(buf, "2:bar\n") == 0);

  /* multiple matches get correct line numbers */
  char *a2[] = { "/bin/grep", "-n", "x", NULL };
  grep_stdin(a2, "x\ny\nx\n", buf, sizeof(buf));
  result("grep -n multiple matches", strcmp(buf, "1:x\n3:x\n") == 0);
}

/* -----------------------------------------------------------------------
 * -v flag: invert match
 * -------------------------------------------------------------------- */
static void test_flag_v(void) {
  printf("Test: -v (invert)\n");
  char buf[256];

  char *a1[] = { "/bin/grep", "-v", "bar", NULL };
  grep_stdin(a1, "foo\nbar\nbaz\n", buf, sizeof(buf));
  result("grep -v excludes matching line", strcmp(buf, "foo\nbaz\n") == 0);

  /* -v with no match → all lines printed, exit 0 */
  char *a2[] = { "/bin/grep", "-v", "xyz", NULL };
  int st = grep_stdin(a2, "foo\nbar\n", buf, sizeof(buf));
  result("grep -v no match: exits 0", child_ok(st));
  result("grep -v no match: all lines", strcmp(buf, "foo\nbar\n") == 0);

  /* -v all match → no output, exit 1 */
  char *a3[] = { "/bin/grep", "-v", ".", NULL };
  st = grep_stdin(a3, "foo\n", buf, sizeof(buf));
  result("grep -v all match: exits 1", child_exit(st) == 1);
}

/* -----------------------------------------------------------------------
 * -i flag: case-insensitive
 * -------------------------------------------------------------------- */
static void test_flag_i(void) {
  printf("Test: -i (case-insensitive)\n");
  char buf[256];

  char *a1[] = { "/bin/grep", "-i", "FOO", NULL };
  grep_stdin(a1, "foo\nbar\nFOO\n", buf, sizeof(buf));
  result("grep -i matches both cases", strcmp(buf, "foo\nFOO\n") == 0);

  char *a2[] = { "/bin/grep", "-i", "hello", NULL };
  grep_stdin(a2, "HELLO\nworld\n", buf, sizeof(buf));
  result("grep -i uppercase pattern matches lowercase", strcmp(buf, "HELLO\n") == 0);
}

/* -----------------------------------------------------------------------
 * -c flag: count
 * -------------------------------------------------------------------- */
static void test_flag_c(void) {
  printf("Test: -c (count)\n");
  char buf[64];

  char *a1[] = { "/bin/grep", "-c", "a", NULL };
  grep_stdin(a1, "apple\nbanana\ncherry\n", buf, sizeof(buf));
  result("grep -c count matches", strcmp(buf, "2\n") == 0);

  char *a2[] = { "/bin/grep", "-c", "xyz", NULL };
  grep_stdin(a2, "foo\nbar\n", buf, sizeof(buf));
  result("grep -c zero matches prints 0", strcmp(buf, "0\n") == 0);
}

/* -----------------------------------------------------------------------
 * -l flag: filename only
 * -------------------------------------------------------------------- */
static void test_flag_l(void) {
  printf("Test: -l (filename only)\n");
  char buf[256];

  write_file("/tmp/tg_l_match",    "hello world\n");
  write_file("/tmp/tg_l_nomatch",  "nothing here\n");

  char *a1[] = { "/bin/grep", "-l", "hello",
                 "/tmp/tg_l_match", "/tmp/tg_l_nomatch", NULL };
  run_capture(a1, buf, sizeof(buf));
  result("grep -l prints only matching filename", strcmp(buf, "/tmp/tg_l_match\n") == 0);

  unlink("/tmp/tg_l_match");
  unlink("/tmp/tg_l_nomatch");
}

/* -----------------------------------------------------------------------
 * Multiple files: filename prefix in output
 * -------------------------------------------------------------------- */
static void test_multi_file(void) {
  printf("Test: multiple files\n");
  char buf[512];

  write_file("/tmp/tg_mf_a", "hit\nmiss\n");
  write_file("/tmp/tg_mf_b", "miss\nhit\n");

  char *a1[] = { "/bin/grep", "hit",
                 "/tmp/tg_mf_a", "/tmp/tg_mf_b", NULL };
  run_capture(a1, buf, sizeof(buf));
  result("grep multi-file: filename prefix in output",
         strstr(buf, "/tmp/tg_mf_a:hit") != NULL &&
         strstr(buf, "/tmp/tg_mf_b:hit") != NULL);

  /* -h suppresses filename prefix */
  char *a2[] = { "/bin/grep", "-h", "hit",
                 "/tmp/tg_mf_a", "/tmp/tg_mf_b", NULL };
  run_capture(a2, buf, sizeof(buf));
  result("grep -h: no filename prefix", strstr(buf, "/tmp/tg_mf_a:") == NULL);
  result("grep -h: matching lines still printed", strstr(buf, "hit") != NULL);

  /* -H forces filename prefix even for single file */
  char *a3[] = { "/bin/grep", "-H", "hit", "/tmp/tg_mf_a", NULL };
  run_capture(a3, buf, sizeof(buf));
  result("grep -H: forces filename prefix", strstr(buf, "/tmp/tg_mf_a:hit") != NULL);

  unlink("/tmp/tg_mf_a");
  unlink("/tmp/tg_mf_b");
}

/* -----------------------------------------------------------------------
 * Extended regex (-E)
 * -------------------------------------------------------------------- */
static void test_flag_E(void) {
  printf("Test: -E (extended regex)\n");
  char buf[256];

  /* alternation: foo|bar */
  char *a1[] = { "/bin/grep", "-E", "foo|bar", NULL };
  grep_stdin(a1, "foo\nbar\nbaz\n", buf, sizeof(buf));
  result("grep -E alternation", strcmp(buf, "foo\nbar\n") == 0);

  /* + quantifier */
  char *a2[] = { "/bin/grep", "-E", "a+", NULL };
  grep_stdin(a2, "b\naa\nc\n", buf, sizeof(buf));
  result("grep -E plus quantifier", strcmp(buf, "aa\n") == 0);

  /* ? quantifier */
  char *a3[] = { "/bin/grep", "-E", "^colou?r$", NULL };
  grep_stdin(a3, "color\ncolour\ncolouur\n", buf, sizeof(buf));
  result("grep -E optional char", strcmp(buf, "color\ncolour\n") == 0);
}

/* -----------------------------------------------------------------------
 * Anchors and dot
 * -------------------------------------------------------------------- */
static void test_regex_anchors(void) {
  printf("Test: anchors and dot\n");
  char buf[256];

  /* ^ anchor */
  char *a1[] = { "/bin/grep", "^foo", NULL };
  grep_stdin(a1, "foobar\nbarfoo\n", buf, sizeof(buf));
  result("grep ^ anchor", strcmp(buf, "foobar\n") == 0);

  /* $ anchor */
  char *a2[] = { "/bin/grep", "foo$", NULL };
  grep_stdin(a2, "foobar\nbarfoo\n", buf, sizeof(buf));
  result("grep $ anchor", strcmp(buf, "barfoo\n") == 0);

  /* . wildcard */
  char *a3[] = { "/bin/grep", "f.o", NULL };
  grep_stdin(a3, "foo\nfxo\nfo\n", buf, sizeof(buf));
  result("grep . matches any char", strcmp(buf, "foo\nfxo\n") == 0);

  /* * quantifier */
  char *a4[] = { "/bin/grep", "^ab*c$", NULL };
  grep_stdin(a4, "ac\nabc\nabbc\nad\n", buf, sizeof(buf));
  result("grep * quantifier", strcmp(buf, "ac\nabc\nabbc\n") == 0);
}

/* -----------------------------------------------------------------------
 * -n combined with -v
 * -------------------------------------------------------------------- */
static void test_combined_flags(void) {
  printf("Test: combined flags\n");
  char buf[256];

  /* -n -v: line numbers on non-matching lines */
  char *a1[] = { "/bin/grep", "-n", "-v", "bar", NULL };
  grep_stdin(a1, "foo\nbar\nbaz\n", buf, sizeof(buf));
  result("grep -n -v: numbered non-matching lines", strcmp(buf, "1:foo\n3:baz\n") == 0);

  /* -i -c: case-insensitive count */
  char *a2[] = { "/bin/grep", "-i", "-c", "hello", NULL };
  grep_stdin(a2, "hello\nHELLO\nworld\n", buf, sizeof(buf));
  result("grep -i -c: count case-insensitive", strcmp(buf, "2\n") == 0);
}

/* -----------------------------------------------------------------------
 * Recursive (-r)
 * -------------------------------------------------------------------- */
static void test_flag_r(void) {
  printf("Test: -r (recursive)\n");
  char buf[512];

  /* Build /tmp/tg_r/{a.txt,sub/b.txt} */
  char *m1[] = { "/bin/mkdir", "-p", "/tmp/tg_r/sub", NULL };
  {
    pid_t pid = fork();
    if (pid == 0) {
      execve("/bin/mkdir", m1, environ);
      _exit(127);
    }
    int st; waitpid(pid, &st, 0);
  }
  write_file("/tmp/tg_r/a.txt",     "needle\nhay\n");
  write_file("/tmp/tg_r/sub/b.txt", "hay\nneedle\n");

  char *a1[] = { "/bin/grep", "-r", "needle", "/tmp/tg_r", NULL };
  int st = run_capture(a1, buf, sizeof(buf));
  result("grep -r exits 0 on match", child_ok(st));
  result("grep -r finds in nested file",
         strstr(buf, "needle") != NULL);
  result("grep -r matches from both files",
         strstr(buf, "/tmp/tg_r/a.txt") != NULL &&
         strstr(buf, "/tmp/tg_r/sub/b.txt") != NULL);

  /* no match → exit 1 */
  char *a2[] = { "/bin/grep", "-r", "xyz", "/tmp/tg_r", NULL };
  st = run_exit(a2);
  result("grep -r no match: exits 1", child_exit(st) == 1);

  /* cleanup */
  unlink("/tmp/tg_r/a.txt");
  unlink("/tmp/tg_r/sub/b.txt");
  char *rd1[] = { "/bin/rmdir", "/tmp/tg_r/sub", NULL };
  char *rd2[] = { "/bin/rmdir", "/tmp/tg_r",     NULL };
  run_exit(rd1); run_exit(rd2);
}

/* -----------------------------------------------------------------------
 * Edge cases
 * -------------------------------------------------------------------- */
static void test_edge_cases(void) {
  printf("Test: edge cases\n");
  char buf[256];

  /* empty input: no match, exit 1 */
  char *a1[] = { "/bin/grep", "foo", NULL };
  int st = grep_stdin(a1, "", buf, sizeof(buf));
  result("grep empty input exits 1", child_exit(st) == 1);
  result("grep empty input: no output", strcmp(buf, "") == 0);

  /* pattern matches every line */
  char *a2[] = { "/bin/grep", ".", NULL };
  st = grep_stdin(a2, "a\nb\nc\n", buf, sizeof(buf));
  result("grep .* matches all lines exits 0", child_ok(st));
  result("grep .* all lines in output", strcmp(buf, "a\nb\nc\n") == 0);

  /* line without trailing newline still matched */
  char *a3[] = { "/bin/grep", "end", NULL };
  grep_stdin(a3, "the end", buf, sizeof(buf));
  result("grep matches line without trailing newline", strstr(buf, "the end") != NULL);

  /* bad pattern exits 2 */
  char *a4[] = { "/bin/grep", "[invalid", NULL };
  st = grep_stdin(a4, "anything\n", buf, sizeof(buf));
  result("grep bad pattern exits 2", child_exit(st) == 2);
}

/* -----------------------------------------------------------------------
 * -- end of flags
 * -------------------------------------------------------------------- */
static void test_double_dash(void) {
  printf("Test: -- end-of-flags\n");
  char buf[256];

  write_file("/tmp/tg_dd", "-flag\nword\n");

  /* grep -- -flag file: treats -flag as pattern, not a flag */
  char *a1[] = { "/bin/grep", "--", "-flag", "/tmp/tg_dd", NULL };
  int st = run_capture(a1, buf, sizeof(buf));
  result("grep -- treats next arg as pattern", child_ok(st));
  result("grep -- matches literal -flag", strcmp(buf, "-flag\n") == 0);

  unlink("/tmp/tg_dd");
}

/* -----------------------------------------------------------------------
 * -c with multiple files
 * -------------------------------------------------------------------- */
static void test_count_multi_file(void) {
  printf("Test: -c with multiple files\n");
  char buf[256];

  write_file("/tmp/tg_cm_a", "hit\nhit\nmiss\n");
  write_file("/tmp/tg_cm_b", "miss\nhit\n");

  char *a1[] = { "/bin/grep", "-c", "hit",
                 "/tmp/tg_cm_a", "/tmp/tg_cm_b", NULL };
  run_capture(a1, buf, sizeof(buf));
  result("grep -c multi-file: per-file counts with prefix",
         strstr(buf, "/tmp/tg_cm_a:2") != NULL &&
         strstr(buf, "/tmp/tg_cm_b:1") != NULL);

  unlink("/tmp/tg_cm_a");
  unlink("/tmp/tg_cm_b");
}

int main(void) {
  printf("=== grep tests ===\n");
  test_basic_match();
  test_file_match();
  test_flag_n();
  test_flag_v();
  test_flag_i();
  test_flag_c();
  test_flag_l();
  test_multi_file();
  test_flag_E();
  test_regex_anchors();
  test_combined_flags();
  test_flag_r();
  test_edge_cases();
  test_double_dash();
  test_count_multi_file();
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
