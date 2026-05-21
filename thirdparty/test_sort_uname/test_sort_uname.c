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


static int run_capture(char *const argv[], char *buf, size_t bufsz) {
  int out_fds[2];
  pipe(out_fds);
  pid_t pid = fork();
  if (pid == 0) {
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

static int run_exit(char *const argv[]) {
  char buf[4];
  return run_capture(argv, buf, sizeof(buf));
}

static int sort_stdin(const char *input, char *const argv[], char *buf, size_t bufsz) {
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
  write(in_fds[1], input, strlen(input));
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

static int write_file(const char *path, const char *content) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return -1;
  size_t len = strlen(content);
  ssize_t n = write(fd, content, len);
  close(fd);
  return (n == (ssize_t)len) ? 0 : -1;
}

/* -----------------------------------------------------------------------
 * uname tests
 * -------------------------------------------------------------------- */
static void test_uname(void) {
  printf("Test: uname\n");
  char buf[256];

  /* default (no args) prints sysname */
  char *a1[] = { "/bin/uname", NULL };
  int st = run_capture(a1, buf, sizeof(buf));
  result("uname exits 0", child_ok(st));
  result("uname default output non-empty", strlen(buf) > 1);
  result("uname default ends with newline", buf[strlen(buf)-1] == '\n');

  /* -s prints sysname = "sbunix" */
  char *a2[] = { "/bin/uname", "-s", NULL };
  run_capture(a2, buf, sizeof(buf));
  size_t len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("uname -s prints sysname", strcmp(buf, "sbunix") == 0);

  /* -m prints machine = "riscv64" */
  char *a3[] = { "/bin/uname", "-m", NULL };
  run_capture(a3, buf, sizeof(buf));
  len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("uname -m prints machine", strcmp(buf, "riscv64") == 0);

  /* -r prints release (non-empty) */
  char *a4[] = { "/bin/uname", "-r", NULL };
  run_capture(a4, buf, sizeof(buf));
  result("uname -r prints release non-empty", strlen(buf) > 1);

  /* -n prints nodename (non-empty) */
  char *a5[] = { "/bin/uname", "-n", NULL };
  run_capture(a5, buf, sizeof(buf));
  result("uname -n prints nodename non-empty", strlen(buf) > 1);

  /* -v prints version (non-empty) */
  char *a6[] = { "/bin/uname", "-v", NULL };
  run_capture(a6, buf, sizeof(buf));
  result("uname -v prints version non-empty", strlen(buf) > 1);

  /* -a prints all fields space-separated on one line */
  char *a7[] = { "/bin/uname", "-a", NULL };
  run_capture(a7, buf, sizeof(buf));
  result("uname -a output contains sysname",  strstr(buf, "sbunix")  != NULL);
  result("uname -a output contains machine",  strstr(buf, "riscv64") != NULL);
  /* -a should have at least 4 spaces (5 fields) */
  int spaces = 0;
  for (size_t k = 0; k < strlen(buf); k++) if (buf[k] == ' ') spaces++;
  result("uname -a has multiple fields", spaces >= 4);

  /* combining flags: -sm prints sysname machine */
  char *a8[] = { "/bin/uname", "-sm", NULL };
  run_capture(a8, buf, sizeof(buf));
  result("uname -sm contains sysname and machine",
         strstr(buf, "sbunix") != NULL && strstr(buf, "riscv64") != NULL);

  /* unknown flag exits non-zero */
  char *a9[] = { "/bin/uname", "-z", NULL };
  st = run_exit(a9);
  result("uname unknown flag exits non-zero", WIFEXITED(st) && WEXITSTATUS(st) != 0);
}

/* -----------------------------------------------------------------------
 * sort: basic alphabetic
 * -------------------------------------------------------------------- */
static void test_sort_basic(void) {
  printf("Test: sort basic\n");
  char buf[256];

  char *a1[] = { "/bin/sort", NULL };
  int st = sort_stdin("banana\napple\ncherry\n", a1, buf, sizeof(buf));
  result("sort exits 0", child_ok(st));
  result("sort alphabetic order", strcmp(buf, "apple\nbanana\ncherry\n") == 0);

  /* already sorted */
  char *a2[] = { "/bin/sort", NULL };
  sort_stdin("a\nb\nc\n", a2, buf, sizeof(buf));
  result("sort already sorted stays sorted", strcmp(buf, "a\nb\nc\n") == 0);

  /* single line */
  char *a3[] = { "/bin/sort", NULL };
  sort_stdin("only\n", a3, buf, sizeof(buf));
  result("sort single line", strcmp(buf, "only\n") == 0);

  /* empty input */
  char *a4[] = { "/bin/sort", NULL };
  st = sort_stdin("", a4, buf, sizeof(buf));
  result("sort empty input exits 0", child_ok(st));
  result("sort empty input: no output", strcmp(buf, "") == 0);
}

/* -----------------------------------------------------------------------
 * sort: -r reverse
 * -------------------------------------------------------------------- */
static void test_sort_reverse(void) {
  printf("Test: sort -r\n");
  char buf[256];

  char *a1[] = { "/bin/sort", "-r", NULL };
  sort_stdin("banana\napple\ncherry\n", a1, buf, sizeof(buf));
  result("sort -r reverse order", strcmp(buf, "cherry\nbanana\napple\n") == 0);
}

/* -----------------------------------------------------------------------
 * sort: -n numeric
 * -------------------------------------------------------------------- */
static void test_sort_numeric(void) {
  printf("Test: sort -n\n");
  char buf[256];

  char *a1[] = { "/bin/sort", "-n", NULL };
  sort_stdin("10\n2\n30\n1\n", a1, buf, sizeof(buf));
  result("sort -n numeric order", strcmp(buf, "1\n2\n10\n30\n") == 0);

  /* -rn: reverse numeric */
  char *a2[] = { "/bin/sort", "-rn", NULL };
  sort_stdin("10\n2\n30\n1\n", a2, buf, sizeof(buf));
  result("sort -rn reverse numeric", strcmp(buf, "30\n10\n2\n1\n") == 0);
}

/* -----------------------------------------------------------------------
 * sort: -u unique
 * -------------------------------------------------------------------- */
static void test_sort_unique(void) {
  printf("Test: sort -u\n");
  char buf[256];

  char *a1[] = { "/bin/sort", "-u", NULL };
  sort_stdin("b\na\nb\na\nc\n", a1, buf, sizeof(buf));
  result("sort -u deduplicates", strcmp(buf, "a\nb\nc\n") == 0);

  /* -un: unique numeric */
  char *a2[] = { "/bin/sort", "-un", NULL };
  sort_stdin("3\n1\n2\n1\n3\n", a2, buf, sizeof(buf));
  result("sort -un unique numeric", strcmp(buf, "1\n2\n3\n") == 0);
}

/* -----------------------------------------------------------------------
 * sort: from file
 * -------------------------------------------------------------------- */
static void test_sort_file(void) {
  printf("Test: sort from file\n");
  char buf[256];

  write_file("/tmp/ts_sort", "cherry\napple\nbanana\n");

  char *a1[] = { "/bin/sort", "/tmp/ts_sort", NULL };
  int st = run_capture(a1, buf, sizeof(buf));
  result("sort file exits 0", child_ok(st));
  result("sort file alphabetic", strcmp(buf, "apple\nbanana\ncherry\n") == 0);

  /* missing file exits non-zero */
  char *a2[] = { "/bin/sort", "/tmp/no_such_sort_xyz", NULL };
  st = run_exit(a2);
  result("sort missing file exits non-zero", WIFEXITED(st) && WEXITSTATUS(st) != 0);

  /* two files concatenated and sorted */
  write_file("/tmp/ts_sort_a", "dog\ncat\n");
  write_file("/tmp/ts_sort_b", "ant\nbee\n");
  char *a3[] = { "/bin/sort", "/tmp/ts_sort_a", "/tmp/ts_sort_b", NULL };
  run_capture(a3, buf, sizeof(buf));
  result("sort two files merged and sorted",
         strcmp(buf, "ant\nbee\ncat\ndog\n") == 0);

  unlink("/tmp/ts_sort");
  unlink("/tmp/ts_sort_a");
  unlink("/tmp/ts_sort_b");
}

int main(void) {
  printf("=== sort + uname tests ===\n");
  test_uname();
  test_sort_basic();
  test_sort_reverse();
  test_sort_numeric();
  test_sort_unique();
  test_sort_file();
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
