#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
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

/* -----------------------------------------------------------------------
 * 1. Basic read/write — parent writes, child reads
 * -------------------------------------------------------------------- */
static void test_basic_read_write(void) {
  printf("Test: basic read/write\n");
  int fds[2];
  result("pipe() succeeds", pipe(fds) == 0);

  pid_t pid = fork();
  if (pid == 0) {
    close(fds[1]);
    char buf[32];
    int n = read(fds[0], buf, sizeof(buf) - 1);
    close(fds[0]);
    exit(n == 5 && memcmp(buf, "hello", 5) == 0 ? 0 : 1);
  }
  close(fds[0]);
  write(fds[1], "hello", 5);
  close(fds[1]);

  int status;
  waitpid(pid, &status, 0);
  result("child received correct data", child_ok(status));
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
    close(fds[1]);
    char buf[32];
    int n = read(fds[0], buf, sizeof(buf));
    close(fds[0]);
    exit(n == 0 ? 0 : 1);
  }
  close(fds[0]);
  close(fds[1]);

  int status;
  waitpid(pid, &status, 0);
  result("read returns 0 after writer closes", child_ok(status));
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
    exit(0);
  }
  close(fds[1]);
  char buf[32];
  int n = read(fds[0], buf, sizeof(buf) - 1);
  close(fds[0]);

  waitpid(pid, NULL, 0);
  result("parent received data written by child",
         n == 5 && memcmp(buf, "world", 5) == 0);
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
    close(fds[1]);
    dup2(fds[0], 0);
    close(fds[0]);
    char buf[32];
    int n = read(0, buf, sizeof(buf) - 1);
    exit(n == 4 && memcmp(buf, "test", 4) == 0 ? 0 : 1);
  }
  close(fds[0]);

  /* Save stdout with dup(), dup write end over stdout, write, restore. */
  int saved_stdout = dup(1);
  dup2(fds[1], 1);
  close(fds[1]);
  write(1, "test", 4);
  dup2(saved_stdout, 1);
  close(saved_stdout);

  int status;
  waitpid(pid, &status, 0);
  result("child read data written via dup2'd stdout", child_ok(status));
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
    int total = 0, n;
    while (total < 15 && (n = read(fds[0], buf + total, sizeof(buf) - total)) > 0)
      total += n;
    close(fds[0]);
    exit(total == 15 && memcmp(buf, "one  two  three", 15) == 0 ? 0 : 1);
  }
  close(fds[0]);
  write(fds[1], "one  ", 5);
  write(fds[1], "two  ", 5);
  write(fds[1], "three", 5);
  close(fds[1]);

  int status;
  waitpid(pid, &status, 0);
  result("all bytes from multiple writes received", child_ok(status));
}

/* -----------------------------------------------------------------------
 * 6. EPIPE — write to pipe with no readers returns EPIPE
 *
 * Parent and child each hold a copy of the write end after fork.
 * Child closes the read end.  Parent closes both ends.  Child then writes —
 * with no readers left, write must fail with EPIPE (and SIGPIPE is
 * ignored so the process is not killed).
 * -------------------------------------------------------------------- */
static void test_epipe(void) {
  printf("Test: EPIPE\n");
  int fds[2];
  pipe(fds);

  /* Ignore SIGPIPE so write() returns -EPIPE instead of killing the child. */
  struct sigaction sa = {0};
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGPIPE, &sa, NULL);

  pid_t pid = fork();
  if (pid == 0) {
    /* Close read end in child — now no readers anywhere after parent closes. */
    close(fds[0]);
    ssize_t r = write(fds[1], "x", 1);
    int got_epipe = (r < 0 && errno == EPIPE);
    close(fds[1]);
    exit(got_epipe ? 0 : 1);
  }

  /* Parent: close both ends so no readers exist before child writes. */
  close(fds[0]);
  close(fds[1]);

  int status;
  waitpid(pid, &status, 0);

  /* Restore SIGPIPE default. */
  sa.sa_handler = SIG_DFL;
  sigaction(SIGPIPE, &sa, NULL);

  result("write to pipe with no readers returns EPIPE", child_ok(status));
}

/* -----------------------------------------------------------------------
 * 7. Pipeline simulation — left child writes, right child reads
 * -------------------------------------------------------------------- */
static void test_pipeline_simulation(void) {
  printf("Test: pipeline simulation\n");
  int fds[2];
  pipe(fds);

  pid_t left = fork();
  if (left == 0) {
    close(fds[0]);
    write(fds[1], "hello", 5);
    close(fds[1]);
    exit(0);
  }

  pid_t right = fork();
  if (right == 0) {
    close(fds[1]);
    char buf[32];
    int n = read(fds[0], buf, sizeof(buf) - 1);
    close(fds[0]);
    exit(n == 5 && memcmp(buf, "hello", 5) == 0 ? 0 : 1);
  }

  close(fds[0]);
  close(fds[1]);

  int sl, sr;
  waitpid(left,  &sl, 0);
  waitpid(right, &sr, 0);
  result("pipeline: right child received left child's data",
         child_ok(sl) && child_ok(sr));
}

/* -----------------------------------------------------------------------
 * 8. dup2 chain — all copies of write end closed, EOF fires
 * -------------------------------------------------------------------- */
static void test_dup2_chain_eof(void) {
  printf("Test: dup2 chain — all copies closed triggers EOF\n");
  int fds[2];
  pipe(fds);

  dup2(fds[1], 5);
  dup2(fds[1], 6);
  dup2(fds[1], 7);
  close(fds[1]);

  pid_t pid = fork();
  if (pid == 0) {
    /* Child inherits fds 5,6,7 — close them all. */
    close(fds[0]);
    close(5); close(6); close(7);
    exit(0);
  }

  /* Parent closes its copies too — now all write ends are gone. */
  close(5); close(6); close(7);

  char buf[8];
  int n = read(fds[0], buf, sizeof(buf));
  close(fds[0]);
  waitpid(pid, NULL, 0);
  result("reader gets EOF after all dup'd write ends closed", n == 0);
}

/* -----------------------------------------------------------------------
 * 9. Large data — write > pipe buffer, reader drains while writer blocks
 * -------------------------------------------------------------------- */
static void test_large_data(void) {
  printf("Test: large data (> pipe buffer)\n");
  int fds[2];
  pipe(fds);

  int total_bytes = 3 * 2048;

  pid_t pid = fork();
  if (pid == 0) {
    close(fds[1]);
    char buf[256];
    int received = 0, ok = 1;
    while (received < total_bytes) {
      int n = read(fds[0], buf, sizeof(buf));
      if (n <= 0) { ok = 0; break; }
      for (int i = 0; i < n; i++) {
        if (buf[i] != (char)((received + i) & 0xff)) { ok = 0; break; }
      }
      received += n;
    }
    close(fds[0]);
    exit(ok && received == total_bytes ? 0 : 1);
  }

  close(fds[0]);
  char buf[256];
  int sent = 0;
  while (sent < total_bytes) {
    int chunk = 256;
    if (sent + chunk > total_bytes) chunk = total_bytes - sent;
    for (int i = 0; i < chunk; i++)
      buf[i] = (char)((sent + i) & 0xff);
    write(fds[1], buf, chunk);
    sent += chunk;
  }
  close(fds[1]);

  int status;
  waitpid(pid, &status, 0);
  result("all bytes received correctly across pipe buffer boundary",
         child_ok(status));
}

/* -----------------------------------------------------------------------
 * 10. Bidirectional — two pipes for parent<->child two-way communication
 * -------------------------------------------------------------------- */
static void test_bidirectional(void) {
  printf("Test: bidirectional (two pipes)\n");
  int p2c[2], c2p[2];
  pipe(p2c);
  pipe(c2p);

  pid_t pid = fork();
  if (pid == 0) {
    close(p2c[1]); close(c2p[0]);
    char buf[32];
    int n = read(p2c[0], buf, sizeof(buf) - 1);
    close(p2c[0]);
    for (int i = 0; i < n; i++)
      if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 32;
    write(c2p[1], buf, n);
    close(c2p[1]);
    exit(0);
  }

  close(p2c[0]); close(c2p[1]);
  write(p2c[1], "ping", 4);
  close(p2c[1]);

  char buf[32];
  int n = read(c2p[0], buf, sizeof(buf) - 1);
  close(c2p[0]);

  waitpid(pid, NULL, 0);
  result("parent sent, child echoed back uppercased",
         n == 4 && memcmp(buf, "PING", 4) == 0);
}

/* -----------------------------------------------------------------------
 * 11. Partial close — one dup'd copy closed, pipe stays open
 * -------------------------------------------------------------------- */
static void test_dup_partial_close(void) {
  printf("Test: close one dup'd write end, pipe stays open\n");
  int fds[2];
  pipe(fds);

  /* Dup write end to fd 8 in the parent. */
  dup2(fds[1], 8);
  close(fds[1]); /* original write end closed; fd 8 is the surviving copy */

  pid_t pid = fork();
  if (pid == 0) {
    /* Child inherits fd 8. Close parent's view — write on fd 8, then close. */
    close(fds[0]);
    write(8, "still", 5);
    close(8);
    exit(0);
  }

  /* Parent closes its copy of fd 8 — child's copy keeps the pipe open. */
  close(8);

  char buf[32];
  int n = read(fds[0], buf, sizeof(buf) - 1);
  close(fds[0]);

  waitpid(pid, NULL, 0);
  result("data received after partial close of dup'd write end",
         n == 5 && memcmp(buf, "still", 5) == 0);
}

/* -----------------------------------------------------------------------
 * 12. Wrong-end I/O — read from write end and write to read end fail
 * -------------------------------------------------------------------- */
static void test_wrong_end_io(void) {
  printf("Test: read from write end / write to read end fail\n");
  int fds[2];
  pipe(fds);

  ssize_t r = read(fds[1], (char[1]){0}, 1);
  int read_write_end_fails = (r < 0);

  ssize_t w = write(fds[0], "x", 1);
  int write_read_end_fails = (w < 0);

  close(fds[0]);
  close(fds[1]);

  result("read from write end returns error", read_write_end_fails);
  result("write to read end returns error",   write_read_end_fails);
}

/* -----------------------------------------------------------------------
 * 13. O_NONBLOCK — read on empty non-blocking pipe returns EAGAIN
 * -------------------------------------------------------------------- */
static void test_nonblock_read_eagain(void) {
  printf("Test: O_NONBLOCK read on empty pipe returns EAGAIN\n");
  int fds[2];
  pipe(fds);

  int flags = fcntl(fds[0], F_GETFL, 0);
  if (flags < 0 || fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) < 0) {
    printf("  [SKIP] nonblock_read_eagain (fcntl O_NONBLOCK not supported)\n");
    close(fds[0]); close(fds[1]);
    return;
  }

  char buf[4];
  ssize_t r = read(fds[0], buf, sizeof(buf));
  int got_eagain = (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));

  close(fds[0]);
  close(fds[1]);
  result("non-blocking read on empty pipe returns EAGAIN", got_eagain);
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
  test_pipeline_simulation();
  test_dup2_chain_eof();
  test_large_data();
  test_bidirectional();
  test_dup_partial_close();
  test_wrong_end_io();
  test_nonblock_read_eagain();

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
