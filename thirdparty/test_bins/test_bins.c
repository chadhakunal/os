#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
  if (ok) { printf("  [PASS] %s\n", name); passed++; }
  else     { printf("  [FAIL] %s\n", name); failed++; }
}

static int child_ok(int status) {
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* Run argv, capture stdout into buf (null-terminated), return exit status. */
static int run_capture(char *const argv[], char *buf, size_t bufsz) {
  int fds[2];
  pipe(fds);
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    dup2(fds[1], 1);
    close(fds[1]);
    execve(argv[0], argv, NULL);
    _exit(127);
  }
  close(fds[1]);
  size_t total = 0;
  ssize_t n;
  while (total < bufsz - 1 && (n = read(fds[0], buf + total, bufsz - 1 - total)) > 0)
    total += n;
  buf[total] = '\0';
  close(fds[0]);
  int status;
  waitpid(pid, &status, 0);
  return status;
}

/* Run argv, return exit status, discard output. */
static int run_exit(char *const argv[]) {
  char buf[4];
  return run_capture(argv, buf, sizeof(buf));
}

/* -----------------------------------------------------------------------
 * echo
 * -------------------------------------------------------------------- */
static void test_echo(void) {
  printf("Test: echo\n");
  char buf[64];

  char *argv1[] = { "/bin/echo", "hello", NULL };
  run_capture(argv1, buf, sizeof(buf));
  result("echo prints argument with newline", strcmp(buf, "hello\n") == 0);

  char *argv2[] = { "/bin/echo", "foo", "bar", NULL };
  run_capture(argv2, buf, sizeof(buf));
  result("echo joins multiple args with space", strcmp(buf, "foo bar\n") == 0);

  char *argv3[] = { "/bin/echo", "-n", "no newline", NULL };
  run_capture(argv3, buf, sizeof(buf));
  result("echo -n suppresses newline", strcmp(buf, "no newline") == 0);

  char *argv4[] = { "/bin/echo", NULL };
  run_capture(argv4, buf, sizeof(buf));
  result("echo with no args prints empty line", strcmp(buf, "\n") == 0);
}

/* -----------------------------------------------------------------------
 * true / false
 * -------------------------------------------------------------------- */
static void test_true_false(void) {
  printf("Test: true/false\n");

  char *ta[] = { "/bin/true", NULL };
  result("true exits 0", child_ok(run_exit(ta)));

  char *fa[] = { "/bin/false", NULL };
  int fs = run_exit(fa);
  result("false exits non-zero", WIFEXITED(fs) && WEXITSTATUS(fs) != 0);
}

/* -----------------------------------------------------------------------
 * pwd
 * -------------------------------------------------------------------- */
static void test_pwd(void) {
  printf("Test: pwd\n");
  char buf[256];

  char *argv[] = { "/bin/pwd", NULL };
  int status = run_capture(argv, buf, sizeof(buf));
  result("pwd exits 0", child_ok(status));

  /* strip trailing newline */
  size_t len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("pwd output starts with /", buf[0] == '/');
  result("pwd output is non-empty", strlen(buf) > 0);
}

/* -----------------------------------------------------------------------
 * wc
 * -------------------------------------------------------------------- */
static void test_wc(void) {
  printf("Test: wc\n");
  char buf[128];

  /* wc from a file: /etc/rc should produce consistent counts */
  char *argv1[] = { "/bin/wc", "/etc/rc", NULL };
  int status = run_capture(argv1, buf, sizeof(buf));
  result("wc file exits 0", child_ok(status));

  int lines, words, bytes;
  int matched = sscanf(buf, "%d %d %d", &lines, &words, &bytes);
  result("wc file produces 3 counts", matched == 3);
  result("wc file line count > 0", lines > 0);
  result("wc file byte count >= line count", bytes >= lines);

  /* wc via pipe: feed known input */
  int fds[2];
  pipe(fds);
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[1]);
    dup2(fds[0], 0);
    close(fds[0]);
    char *wargv[] = { "/bin/wc", NULL };
    execve("/bin/wc", wargv, NULL);
    _exit(127);
  }
  close(fds[0]);
  const char *input = "one two three\nfour five\n";
  write(fds[1], input, strlen(input));
  close(fds[1]);

  /* capture wc's stdout via a second pipe */
  int fds2[2];
  pipe(fds2);
  /* wc's stdout was inherited from parent — we need a different approach:
   * re-run wc with both stdin and stdout redirected */
  waitpid(pid, NULL, 0);
  close(fds2[0]); close(fds2[1]);

  /* Simpler: fork with both ends redirected */
  int in_fds[2], out_fds[2];
  pipe(in_fds);
  pipe(out_fds);
  pid = fork();
  if (pid == 0) {
    close(in_fds[1]); close(out_fds[0]);
    dup2(in_fds[0],  0); close(in_fds[0]);
    dup2(out_fds[1], 1); close(out_fds[1]);
    char *wargv[] = { "/bin/wc", NULL };
    execve("/bin/wc", wargv, NULL);
    _exit(127);
  }
  close(in_fds[0]); close(out_fds[1]);
  write(in_fds[1], input, strlen(input));
  close(in_fds[1]);

  size_t total = 0;
  ssize_t n;
  while (total < sizeof(buf)-1 && (n = read(out_fds[0], buf+total, sizeof(buf)-1-total)) > 0)
    total += n;
  buf[total] = '\0';
  close(out_fds[0]);
  waitpid(pid, &status, 0);

  result("wc stdin exits 0", child_ok(status));
  matched = sscanf(buf, "%d %d %d", &lines, &words, &bytes);
  result("wc stdin: 2 lines", matched == 3 && lines == 2);
  result("wc stdin: 5 words", words == 5);
  result("wc stdin: correct bytes", bytes == (int)strlen(input));

  /* wc multiple files shows total line */
  char *argv2[] = { "/bin/wc", "/etc/rc", "/etc/rc", NULL };
  run_capture(argv2, buf, sizeof(buf));
  result("wc two files output contains 'total'", strstr(buf, "total") != NULL);
}

/* -----------------------------------------------------------------------
 * kill — just verify it exits cleanly with a valid signal/pid
 * -------------------------------------------------------------------- */
static void test_kill(void) {
  printf("Test: kill\n");

  /* kill with no args should exit non-zero */
  char *argv1[] = { "/bin/kill", NULL };
  int s = run_exit(argv1);
  result("kill with no args exits non-zero", WIFEXITED(s) && WEXITSTATUS(s) != 0);

  /* send SIGCONT to ourselves — harmless */
  char pid_str[16];
  pid_t self = getpid();
  int len = 0;
  unsigned long v = (unsigned long)self;
  if (v == 0) { pid_str[len++] = '0'; }
  else { char tmp[16]; int tl = 0; while (v) { tmp[tl++] = '0' + v%10; v/=10; } while (tl--) pid_str[len++] = tmp[tl+1]; }
  /* simpler: use sprintf */
  snprintf(pid_str, sizeof(pid_str), "%d", (int)self);

  char *argv2[] = { "/bin/kill", "-18", pid_str, NULL }; /* 18 = SIGCONT */
  s = run_exit(argv2);
  result("kill -18 self exits 0", child_ok(s));
}

/* -----------------------------------------------------------------------
 * ps
 * -------------------------------------------------------------------- */
static void test_ps(void) {
  printf("Test: ps\n");
  char buf[1024];

  char *argv[] = { "/bin/ps", NULL };
  int status = run_capture(argv, buf, sizeof(buf));
  result("ps exits 0", child_ok(status));
  result("ps output contains PID header", strstr(buf, "PID") != NULL);

  /* our own pid should appear */
  char pid_str[16];
  snprintf(pid_str, sizeof(pid_str), "%d", (int)getpid());
  result("ps output contains current PID", strstr(buf, pid_str) != NULL);
}

/* -----------------------------------------------------------------------
 * test (the binary)
 * -------------------------------------------------------------------- */
static void test_test(void) {
  printf("Test: test\n");

  char *t1[] = { "/bin/test", "-f", "/etc/rc", NULL };
  result("test -f existing file exits 0", child_ok(run_exit(t1)));

  char *t2[] = { "/bin/test", "-f", "/no/such/file", NULL };
  int s = run_exit(t2);
  result("test -f missing file exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  char *t3[] = { "/bin/test", "-d", "/bin", NULL };
  result("test -d directory exits 0", child_ok(run_exit(t3)));

  char *t4[] = { "/bin/test", "-d", "/etc/rc", NULL };
  s = run_exit(t4);
  result("test -d on file exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  char *t5[] = { "/bin/test", "hello", "=", "hello", NULL };
  result("test string = exits 0", child_ok(run_exit(t5)));

  char *t6[] = { "/bin/test", "foo", "=", "bar", NULL };
  s = run_exit(t6);
  result("test string != exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  char *t7[] = { "/bin/test", "3", "-gt", "2", NULL };
  result("test 3 -gt 2 exits 0", child_ok(run_exit(t7)));

  char *t8[] = { "/bin/test", "1", "-gt", "2", NULL };
  s = run_exit(t8);
  result("test 1 -gt 2 exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  char *t9[] = { "/bin/test", "-z", "", NULL };
  result("test -z empty string exits 0", child_ok(run_exit(t9)));

  char *t10[] = { "/bin/test", "-n", "nonempty", NULL };
  result("test -n nonempty exits 0", child_ok(run_exit(t10)));

  char *t11[] = { "/bin/test", "-e", "/etc/rc", NULL };
  result("test -e existing path exits 0", child_ok(run_exit(t11)));
}

/* -----------------------------------------------------------------------
 * wc piped from echo
 * -------------------------------------------------------------------- */
static void test_wc_pipe_echo(void) {
  printf("Test: wc via pipe from echo\n");
  char buf[64];

  /* echo "a b c" | wc  =>  1 line, 3 words, 6 bytes ("a b c\n") */
  int in_fds[2], out_fds[2];
  pipe(in_fds); pipe(out_fds);

  pid_t pid = fork();
  if (pid == 0) {
    close(in_fds[1]); close(out_fds[0]);
    dup2(in_fds[0],  0); close(in_fds[0]);
    dup2(out_fds[1], 1); close(out_fds[1]);
    char *wargv[] = { "/bin/wc", NULL };
    execve("/bin/wc", wargv, NULL);
    _exit(127);
  }
  close(in_fds[0]); close(out_fds[1]);
  const char *input = "a b c\n";
  write(in_fds[1], input, strlen(input));
  close(in_fds[1]);

  size_t total = 0; ssize_t n;
  while (total < sizeof(buf)-1 && (n = read(out_fds[0], buf+total, sizeof(buf)-1-total)) > 0)
    total += n;
  buf[total] = '\0';
  close(out_fds[0]);

  int status; waitpid(pid, &status, 0);
  int lines, words, bytes;
  int matched = sscanf(buf, "%d %d %d", &lines, &words, &bytes);
  result("echo|wc: 1 line",    matched == 3 && lines == 1);
  result("echo|wc: 3 words",   words == 3);
  result("echo|wc: 6 bytes",   bytes == 6);
}

int main(void) {
  printf("=== bin tests ===\n");
  test_echo();
  test_true_false();
  test_pwd();
  test_wc();
  test_kill();
  test_ps();
  test_test();
  test_wc_pipe_echo();
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
