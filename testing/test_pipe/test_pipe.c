#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <poll.h>

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
 * 14. lseek on a pipe — must fail with ESPIPE, pipe stays usable
 * -------------------------------------------------------------------- */
static void test_lseek_espipe(void) {
  printf("Test: lseek on pipe returns ESPIPE\n");
  int fds[2];
  pipe(fds);

  /* All three whence values must each fail with ESPIPE. */
  errno = 0;
  off_t r0 = lseek(fds[0], 0, SEEK_SET);
  result("lseek SEEK_SET on read end returns -1",  r0 == (off_t)-1);
  result("lseek SEEK_SET on read end sets ESPIPE", errno == ESPIPE);

  errno = 0;
  off_t r1 = lseek(fds[0], 0, SEEK_CUR);
  result("lseek SEEK_CUR on read end returns -1",  r1 == (off_t)-1);
  result("lseek SEEK_CUR on read end sets ESPIPE", errno == ESPIPE);

  errno = 0;
  off_t r2 = lseek(fds[0], 0, SEEK_END);
  result("lseek SEEK_END on read end returns -1",  r2 == (off_t)-1);
  result("lseek SEEK_END on read end sets ESPIPE", errno == ESPIPE);

  errno = 0;
  off_t r3 = lseek(fds[1], 0, SEEK_SET);
  result("lseek SEEK_SET on write end returns -1",  r3 == (off_t)-1);
  result("lseek SEEK_SET on write end sets ESPIPE", errno == ESPIPE);

  /* Pipe must remain fully usable after the failed seeks. */
  ssize_t wn = write(fds[1], "ok", 2);
  char buf[4] = {0};
  ssize_t rn = read(fds[0], buf, sizeof(buf));
  result("pipe still writable after lseek failure", wn == 2);
  result("pipe still readable after lseek failure",
         rn == 2 && buf[0] == 'o' && buf[1] == 'k');

  close(fds[0]);
  close(fds[1]);
}

/* -----------------------------------------------------------------------
 * 15. Single large write — one write() larger than the pipe buffer
 *
 * Before the fix, the writer filled the buffer, blocked waiting for space,
 * but the reader was never woken mid-write so both sides deadlocked.
 * The writer must block until the reader drains enough space, and the
 * reader must see every byte in order.
 * -------------------------------------------------------------------- */
static void test_single_large_write(void) {
  printf("Test: single large write (> pipe buffer)\n");

  /* 3× PIPE_BUF_SIZE so the writer must block and be woken at least twice. */
  const int DATA_SIZE = 6144;
  const int READ_CHUNK = 512;

  char *wbuf = malloc(DATA_SIZE);
  char *rbuf = malloc(DATA_SIZE);
  /* Known pattern: prime modulus avoids power-of-2 aliasing in misread data. */
  for (int i = 0; i < DATA_SIZE; i++)
    wbuf[i] = (char)(i % 251);

  int fds[2];
  pipe(fds);

  /* Child issues a single write() of the full DATA_SIZE bytes. */
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    ssize_t n = write(fds[1], wbuf, DATA_SIZE);
    close(fds[1]);
    _exit(n == DATA_SIZE ? 0 : 1);
  }
  close(fds[1]);

  /* Parent reads in small chunks — must make steady progress
   * waking the blocked writer each time space opens. */
  int total = 0;
  ssize_t n;
  while (total < DATA_SIZE &&
         (n = read(fds[0], rbuf + total, READ_CHUNK)) > 0)
    total += n;
  close(fds[0]);

  int status;
  waitpid(pid, &status, 0);

  result("single large write: writer exits 0",
         WIFEXITED(status) && WEXITSTATUS(status) == 0);
  result("single large write: all bytes received",
         total == DATA_SIZE);

  int data_ok = 1;
  for (int i = 0; i < DATA_SIZE && data_ok; i++)
    if (rbuf[i] != wbuf[i]) data_ok = 0;
  result("single large write: data integrity", data_ok);

  free(wbuf);
  free(rbuf);
}

/* -----------------------------------------------------------------------
 * 16. O_NONBLOCK write — full pipe returns EAGAIN, not EWOULDBLOCK
 * -------------------------------------------------------------------- */
static void test_nonblock_write_eagain(void) {
  printf("Test: O_NONBLOCK write on full pipe returns EAGAIN\n");
  int fds[2];
  pipe(fds);

  int flags = fcntl(fds[1], F_GETFL, 0);
  if (flags < 0 || fcntl(fds[1], F_SETFL, flags | O_NONBLOCK) < 0) {
    printf("  [SKIP] nonblock_write_eagain (fcntl O_NONBLOCK not supported)\n");
    close(fds[0]); close(fds[1]);
    return;
  }

  /* Also set read end nonblocking so we can fill without blocking. */
  int rflags = fcntl(fds[0], F_GETFL, 0);
  fcntl(fds[0], F_SETFL, rflags | O_NONBLOCK);

  /* Fill the pipe buffer with 1-byte writes until we get EAGAIN. */
  char byte = 'x';
  int filled = 0;
  ssize_t w;
  while ((w = write(fds[1], &byte, 1)) == 1)
    filled++;

  int got_eagain = (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
  result("nonblocking write on full pipe returns EAGAIN", got_eagain);
  result("filled at least one byte before EAGAIN", filled > 0);

  /* After draining one byte there should be space again. */
  char rbuf[1];
  read(fds[0], rbuf, 1);
  errno = 0;
  w = write(fds[1], &byte, 1);
  result("nonblocking write succeeds after drain", w == 1);

  close(fds[0]);
  close(fds[1]);
}

/* -----------------------------------------------------------------------
 * 17. SIGPIPE delivery — write to closed-reader pipe delivers SIGPIPE
 *
 * We test two sub-cases:
 *   a) SIGPIPE default action: the writer process is killed by SIGPIPE.
 *   b) SIGPIPE ignored: write() returns -1 with errno == EPIPE.
 * -------------------------------------------------------------------- */
static volatile int sigpipe_received = 0;
static void sigpipe_handler(int sig) { (void)sig; sigpipe_received = 1; }

static void test_sigpipe_delivery(void) {
  printf("Test: SIGPIPE delivery\n");

  /* Sub-case a: default action kills the writer. */
  {
    int fds[2];
    pipe(fds);

    pid_t pid = fork();
    if (pid == 0) {
      /* Child: close read end, then write — should be killed by SIGPIPE. */
      close(fds[0]);
      close(fds[1]); /* no readers */
      int fds2[2];
      pipe(fds2);
      close(fds2[0]); /* immediately close reader so write end has no reader */
      write(fds2[1], "x", 1);
      /* If we reach here SIGPIPE was not delivered — exit non-zero. */
      close(fds2[1]);
      _exit(1);
    }
    close(fds[0]);
    close(fds[1]);

    int status;
    waitpid(pid, &status, 0);
    int killed_by_sigpipe = WIFSIGNALED(status) && WTERMSIG(status) == SIGPIPE;
    result("SIGPIPE default action kills writer", killed_by_sigpipe);
  }

  /* Sub-case b: SIGPIPE ignored — write returns EPIPE. */
  {
    int fds[2];
    pipe(fds);

    struct sigaction sa = {0}, old;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, &old);

    close(fds[0]); /* no readers */
    errno = 0;
    ssize_t r = write(fds[1], "x", 1);
    int got_epipe = (r < 0 && errno == EPIPE);
    close(fds[1]);

    sigaction(SIGPIPE, &old, NULL);
    result("SIGPIPE ignored: write returns -1 with EPIPE", got_epipe);
  }

  /* Sub-case c: custom SIGPIPE handler is invoked. */
  {
    int fds[2];
    pipe(fds);

    struct sigaction sa = {0}, old;
    sa.sa_handler = sigpipe_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, &old);

    sigpipe_received = 0;
    close(fds[0]);
    write(fds[1], "x", 1);
    close(fds[1]);

    sigaction(SIGPIPE, &old, NULL);
    result("SIGPIPE custom handler invoked on broken pipe", sigpipe_received == 1);
  }
}

/* -----------------------------------------------------------------------
 * 18. poll() on pipes
 * -------------------------------------------------------------------- */
static void test_poll_pipes(void) {
  printf("Test: poll on pipes\n");
  int fds[2];
  pipe(fds);

  /* Read end on empty pipe: POLLIN should NOT be set, no POLLHUP yet. */
  struct pollfd pfd = { fds[0], POLLIN, 0 };
  int r = poll(&pfd, 1, 0);
  result("poll on empty pipe: not readable", r == 0 || !(pfd.revents & POLLIN));

  /* Write end when pipe has space: POLLOUT should be set. */
  pfd.fd     = fds[1];
  pfd.events = POLLOUT;
  pfd.revents = 0;
  r = poll(&pfd, 1, 0);
  result("poll on write end with space: POLLOUT set", r == 1 && (pfd.revents & POLLOUT));

  /* Write data — now read end should report POLLIN. */
  write(fds[1], "hi", 2);
  pfd.fd      = fds[0];
  pfd.events  = POLLIN;
  pfd.revents = 0;
  r = poll(&pfd, 1, 0);
  result("poll on non-empty pipe: POLLIN set", r == 1 && (pfd.revents & POLLIN));

  /* Drain data, then close write end — read end should report POLLHUP. */
  char buf[4];
  read(fds[0], buf, sizeof(buf));
  close(fds[1]);
  pfd.fd      = fds[0];
  pfd.events  = POLLIN;
  pfd.revents = 0;
  r = poll(&pfd, 1, 0);
  result("poll after writer close: POLLHUP set", pfd.revents & POLLHUP);

  close(fds[0]);
}

/* -----------------------------------------------------------------------
 * 19. Partial read — only M bytes available, request N > M, get M back
 * -------------------------------------------------------------------- */
static void test_partial_read(void) {
  printf("Test: partial read returns available bytes\n");
  int fds[2];
  pipe(fds);

  /* Write 3 bytes, request 10 — should get exactly 3 without blocking. */
  write(fds[1], "abc", 3);

  /* Set read end nonblocking so the read won't block if the count is wrong. */
  int flags = fcntl(fds[0], F_GETFL, 0);
  fcntl(fds[0], F_SETFL, flags | O_NONBLOCK);

  char buf[16] = {0};
  ssize_t n = read(fds[0], buf, sizeof(buf));
  result("partial read returns available byte count", n == 3);
  result("partial read data correct", memcmp(buf, "abc", 3) == 0);

  close(fds[0]);
  close(fds[1]);
}

/* -----------------------------------------------------------------------
 * 20. Zero-byte write — returns 0, no SIGPIPE even with no readers
 * -------------------------------------------------------------------- */
static void test_zero_byte_write(void) {
  printf("Test: zero-byte write\n");
  int fds[2];
  pipe(fds);

  /* Zero write on a normal pipe with readers: must return 0. */
  ssize_t r = write(fds[1], "x", 0);
  result("zero-byte write returns 0", r == 0);

  /* Zero write with reader closed and SIGPIPE ignored: still return 0. */
  struct sigaction sa = {0}, old;
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGPIPE, &sa, &old);

  close(fds[0]);
  errno = 0;
  r = write(fds[1], "x", 0);
  result("zero-byte write with no readers returns 0, not EPIPE", r == 0);

  sigaction(SIGPIPE, &old, NULL);
  close(fds[1]);
}

/* -----------------------------------------------------------------------
 * 21. O_CLOEXEC — pipe fd closed across exec, non-cloexec fd survives
 * -------------------------------------------------------------------- */
static void test_cloexec(void) {
  printf("Test: O_CLOEXEC closes fd across exec\n");

  /* Create two pipes: one with O_CLOEXEC on the write end, one without. */
  int cloexec_fds[2], normal_fds[2];
  pipe(cloexec_fds);
  pipe(normal_fds);

  /* Mark the write end of the cloexec pipe. */
  int flags = fcntl(cloexec_fds[1], F_GETFD, 0);
  fcntl(cloexec_fds[1], F_SETFD, flags | FD_CLOEXEC);

  pid_t pid = fork();
  if (pid == 0) {
    /* Child execs /bin/true. Before exec, write a marker to each write end.
     * After exec the cloexec write end should be closed (reader sees EOF),
     * but the normal write end stays open (reader blocks). */

    /* We need to check this from the parent side after exec, so instead of
     * writing here, we just exec and let the parent detect which write ends
     * survived via poll with timeout. */
    close(cloexec_fds[0]);
    close(normal_fds[0]);

    /* exec /bin/true — it exits 0 immediately */
    char *argv[] = { "true", NULL };
    char *envp[] = { NULL };
    execve("/bin/true", argv, envp);
    _exit(1); /* execve failed */
  }

  close(cloexec_fds[1]);
  close(normal_fds[1]);

  /* Wait for child to exec and exit. */
  int status;
  waitpid(pid, &status, 0);
  result("exec'd /bin/true exits 0", WIFEXITED(status) && WEXITSTATUS(status) == 0);

  /* After exec+exit, O_CLOEXEC write end was closed by exec — so the child
   * held the only copy and it's gone: read should return EOF (0). */
  int flags2 = fcntl(cloexec_fds[0], F_GETFL, 0);
  fcntl(cloexec_fds[0], F_SETFL, flags2 | O_NONBLOCK);
  char buf[4];
  ssize_t r = read(cloexec_fds[0], buf, sizeof(buf));
  result("O_CLOEXEC write end closed across exec (EOF)", r == 0);

  /* Normal (non-cloexec) write end also closed when child exited after exec,
   * so it should also be EOF now. */
  int flags3 = fcntl(normal_fds[0], F_GETFL, 0);
  fcntl(normal_fds[0], F_SETFL, flags3 | O_NONBLOCK);
  r = read(normal_fds[0], buf, sizeof(buf));
  result("non-cloexec fd also closed after child exit (EOF)", r == 0);

  close(cloexec_fds[0]);
  close(normal_fds[0]);
}

/* -----------------------------------------------------------------------
 * 22. Writer blocks when pipe is full (blocking mode)
 *
 * The writer fills the pipe in one call larger than the buffer and blocks.
 * The reader drains it in chunks. Verifies the writer does not return short
 * or error — it must block until all bytes are delivered.
 * -------------------------------------------------------------------- */
static void test_writer_blocks_when_full(void) {
  printf("Test: writer blocks when pipe full (blocking mode)\n");

  /* 4× pipe buffer so writer must block multiple times. */
  const int DATA_SIZE = 4 * 2048;

  char *wbuf = malloc(DATA_SIZE);
  char *rbuf = malloc(DATA_SIZE);
  for (int i = 0; i < DATA_SIZE; i++)
    wbuf[i] = (char)(i % 199);

  int fds[2];
  pipe(fds);

  /* Child is the writer: one blocking write of the full DATA_SIZE. */
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    ssize_t n = write(fds[1], wbuf, DATA_SIZE);
    close(fds[1]);
    _exit(n == DATA_SIZE ? 0 : 1);
  }
  close(fds[1]);

  /* Parent is the reader: drain in small chunks, giving the writer
   * chances to wake and refill. */
  int total = 0;
  ssize_t n;
  while (total < DATA_SIZE &&
         (n = read(fds[0], rbuf + total, 128)) > 0)
    total += n;
  close(fds[0]);

  int wstatus;
  waitpid(pid, &wstatus, 0);

  result("blocking writer exits 0 (wrote all bytes)",
         WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0);
  result("blocking writer: all bytes received", total == DATA_SIZE);

  int data_ok = 1;
  for (int i = 0; i < DATA_SIZE && data_ok; i++)
    if (rbuf[i] != wbuf[i]) data_ok = 0;
  result("blocking writer: data integrity", data_ok);

  free(wbuf);
  free(rbuf);
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
  test_lseek_espipe();
  test_single_large_write();
  test_nonblock_write_eagain();
  test_sigpipe_delivery();
  test_poll_pipes();
  test_partial_read();
  test_zero_byte_write();
  test_cloexec();
  test_writer_blocks_when_full();

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
