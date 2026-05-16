#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
  if (ok) { printf("  [PASS] %s\n", name); passed++; }
  else     { printf("  [FAIL] %s\n", name); failed++; }
}

/* -----------------------------------------------------------------------
 * 1. Basic read/write — parent writes, child reads
 * -------------------------------------------------------------------- */
static void test_basic_read_write(void) {
  printf("Test: basic read/write\n");
  int fds[2];
  result("pipe() succeeds", pipe(fds) == 0);

  pid_t pid = fork();
  if (pid == 0) {
    /* child: read from pipe */
    close(fds[1]);
    char buf[32];
    int n = read(fds[0], buf, sizeof(buf) - 1);
    close(fds[0]);
    if (n == 5 && buf[0] == 'h' && buf[1] == 'e' &&
        buf[2] == 'l' && buf[3] == 'l' && buf[4] == 'o')
      _exit(0);
    _exit(1);
  }
  /* parent: write to pipe */
  close(fds[0]);
  write(fds[1], "hello", 5);
  close(fds[1]);

  int status;
  waitpid(pid, &status, 0);
  result("child received correct data", status == 0);
}

/* -----------------------------------------------------------------------
 * 2. EOF when writer closes
 * -------------------------------------------------------------------- */
static void test_eof_on_writer_close(void) {
  printf("Test: EOF when writer closes\n");
  int fds[2];
  pipe(fds);

  pid_t pid = fork();
  if (pid == 0) {
    /* child: block on read, expect EOF after parent closes */
    close(fds[1]);
    char buf[32];
    int n = read(fds[0], buf, sizeof(buf));
    close(fds[0]);
    /* EOF = 0 bytes */
    _exit(n == 0 ? 0 : 1);
  }
  /* parent: close write end without writing anything */
  close(fds[0]);
  close(fds[1]);

  int status;
  waitpid(pid, &status, 0);
  result("read returns 0 after writer closes", status == 0);
}

/* -----------------------------------------------------------------------
 * 3. Fork inheritance — child writes, parent reads
 * -------------------------------------------------------------------- */
static void test_fork_inheritance(void) {
  printf("Test: fork inheritance\n");
  int fds[2];
  pipe(fds);

  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    write(fds[1], "world", 5);
    close(fds[1]);
    _exit(0);
  }
  close(fds[1]);
  char buf[32];
  int n = read(fds[0], buf, sizeof(buf) - 1);
  close(fds[0]);

  waitpid(pid, NULL, 0);
  result("parent received data written by child",
         n == 5 && buf[0] == 'w' && buf[4] == 'd');
}

/* -----------------------------------------------------------------------
 * 4. dup2 — redirect pipe ends over stdin/stdout
 * -------------------------------------------------------------------- */
static void test_dup2_stdin_stdout(void) {
  printf("Test: dup2 to stdin/stdout\n");
  int fds[2];
  pipe(fds);

  pid_t pid = fork();
  if (pid == 0) {
    /* child: dup read end to stdin, read via fd 0 */
    close(fds[1]);
    dup2(fds[0], 0);
    close(fds[0]);
    char buf[32];
    int n = read(0, buf, sizeof(buf) - 1);
    _exit(n == 4 && buf[0] == 't' && buf[1] == 'e' &&
          buf[2] == 's' && buf[3] == 't' ? 0 : 1);
  }
  /* parent: save stdout, dup write end over it, write, then restore */
  close(fds[0]);
  int saved_stdout = 10;
  dup2(1, saved_stdout);
  dup2(fds[1], 1);
  close(fds[1]);
  write(1, "test", 4);
  dup2(saved_stdout, 1);
  close(saved_stdout);

  int status;
  waitpid(pid, &status, 0);

  result("child read data written via dup2'd stdout", status == 0);
}

/* -----------------------------------------------------------------------
 * 5. Multiple writes — pipe buffers multiple messages
 * -------------------------------------------------------------------- */
static void test_multiple_writes(void) {
  printf("Test: multiple writes\n");
  int fds[2];
  pipe(fds);

  pid_t pid = fork();
  if (pid == 0) {
    close(fds[1]);
    char buf[64];
    int total = 0;
    int n;
    while (total < 15 && (n = read(fds[0], buf + total, sizeof(buf) - total)) > 0)
      total += n;
    close(fds[0]);
    _exit(total == 15 &&
          buf[0]  == 'o' && buf[4]  == 'e' &&
          buf[5]  == 't' && buf[9]  == 'o' &&
          buf[10] == 't' && buf[14] == 'e' ? 0 : 1);
  }
  close(fds[0]);
  write(fds[1], "one  ", 5);
  write(fds[1], "two  ", 5);
  write(fds[1], "three", 5);
  close(fds[1]);

  int status;
  waitpid(pid, &status, 0);
  result("all bytes from multiple writes received", status == 0);
}

/* -----------------------------------------------------------------------
 * 6. EPIPE — write to pipe with no readers
 * -------------------------------------------------------------------- */
static void test_epipe(void) {
  printf("Test: EPIPE\n");
  int fds[2];
  pipe(fds);

  pid_t pid = fork();
  if (pid == 0) {
    /* child: close read end immediately, then write should get -EPIPE */
    close(fds[0]);
    /* give parent a moment to also close its read end */
    int r = write(fds[1], "x", 1);
    close(fds[1]);
    _exit(r < 0 ? 0 : 1);
  }
  /* parent: close both ends so no readers exist */
  close(fds[0]);
  close(fds[1]);

  int status;
  waitpid(pid, &status, 0);
  result("write to pipe with no readers returns error", status == 0);
}

/* -----------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------- */
int main(void) {
  printf("=== pipe tests ===\n");

  test_basic_read_write();
  test_eof_on_writer_close();
  test_fork_inheritance();
  test_dup2_stdin_stdout();
  test_multiple_writes();
  test_epipe();

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
