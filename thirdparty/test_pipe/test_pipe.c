#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
      exit(0);
    exit(1);
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
    exit(n == 0 ? 0 : 1);
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
    exit(0);
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
    exit(n == 4 && buf[0] == 't' && buf[1] == 'e' &&
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
    exit(total == 15 && memcmp(buf, "one  two  three", 15) == 0 ? 0 : 1);
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
    exit(r < 0 ? 0 : 1);
  }
  /* parent: close both ends so no readers exist */
  close(fds[0]);
  close(fds[1]);

  int status;
  waitpid(pid, &status, 0);
  result("write to pipe with no readers returns error", status == 0);
}

/* -----------------------------------------------------------------------
 * 7. Pipeline simulation — left child writes, right child reads+processes
 * -------------------------------------------------------------------- */
static void test_pipeline_simulation(void) {
  printf("Test: pipeline simulation\n");
  int fds[2];
  pipe(fds);

  /* left child: writes to pipe */
  pid_t left = fork();
  if (left == 0) {
    close(fds[0]);
    write(fds[1], "hello", 5);
    close(fds[1]);
    exit(0);
  }

  /* right child: reads from pipe, checks data */
  pid_t right = fork();
  if (right == 0) {
    close(fds[1]);
    char buf[32];
    int n = read(fds[0], buf, sizeof(buf) - 1);
    close(fds[0]);
    exit(n == 5 && buf[0] == 'h' && buf[4] == 'o' ? 0 : 1);
  }

  close(fds[0]);
  close(fds[1]);

  int sl, sr;
  waitpid(left,  &sl, 0);
  waitpid(right, &sr, 0);
  result("pipeline: right child received left child's data",
         sl == 0 && sr == 0);
}

/* -----------------------------------------------------------------------
 * 8. dup2 chain — write end duped multiple times, all closed, EOF fires
 * -------------------------------------------------------------------- */
static void test_dup2_chain_eof(void) {
  printf("Test: dup2 chain — all copies closed triggers EOF\n");
  int fds[2];
  pipe(fds);

  /* dup write end to fds 5, 6, 7 */
  dup2(fds[1], 5);
  dup2(fds[1], 6);
  dup2(fds[1], 7);
  close(fds[1]); /* close original write end */

  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    close(5); close(6); close(7);
    exit(0);
  }

  /* parent: close all dup'd copies */
  close(5);
  close(6);
  close(7);

  /* now reader should get EOF */
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

  /* Use 3x pipe buffer size */
  int total_bytes = 3 * 2048;

  pid_t pid = fork();
  if (pid == 0) {
    /* child: read all bytes, verify pattern */
    close(fds[1]);
    char buf[256];
    int received = 0;
    int ok = 1;
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

  /* parent: write pattern in chunks */
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
         status == 0);
}

/* -----------------------------------------------------------------------
 * 10. Bidirectional — two pipes for parent<->child two-way communication
 * -------------------------------------------------------------------- */
static void test_bidirectional(void) {
  printf("Test: bidirectional (two pipes)\n");
  int p2c[2], c2p[2]; /* parent-to-child, child-to-parent */
  pipe(p2c);
  pipe(c2p);

  pid_t pid = fork();
  if (pid == 0) {
    close(p2c[1]); close(c2p[0]);
    /* read from parent, echo back uppercased */
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
         n == 4 && buf[0] == 'P' && buf[1] == 'I' &&
         buf[2] == 'N' && buf[3] == 'G');
}

/* -----------------------------------------------------------------------
 * 11. Close one dup'd copy — pipe still open, reader still blocks
 * -------------------------------------------------------------------- */
static void test_dup_partial_close(void) {
  printf("Test: close one dup'd write end, pipe stays open\n");
  int fds[2];
  pipe(fds);

  /* dup write end to fd 8 */
  dup2(fds[1], 8);

  pid_t pid = fork();
  if (pid == 0) {
    /* child: close the dup'd copy but keep original, then write, then close */
    close(8);
    write(fds[1], "still", 5);
    close(fds[1]);
    exit(0);
  }

  /* parent: close original write end but fd 8 is in child — wait for data */
  close(fds[1]);
  close(8);

  char buf[32];
  int n = read(fds[0], buf, sizeof(buf) - 1);
  close(fds[0]);

  waitpid(pid, NULL, 0);
  result("data received after partial close of dup'd write end",
         n == 5 && buf[0] == 's' && buf[4] == 'l');
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

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
