#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* Focused /dev/null regression test.
 * Each step prints before AND after so a hang pinpoints the exact syscall. */

static int passed = 0;
static int failed = 0;

#define SAY(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)
#define PASS(t) do { printf("  PASS: %s\n", (t)); fflush(stdout); passed++; } while(0)
#define FAIL(t, ...) do { printf("  FAIL: %s -- ", (t)); printf(__VA_ARGS__); printf("\n"); fflush(stdout); failed++; } while(0)

int main(void) {
  SAY("test_devnull: start\n");

  /* -----------------------------------------------------------------------
   * 1. open /dev/null O_WRONLY — must not hang, must return valid fd
   * -------------------------------------------------------------------- */
  SAY("  [1] open O_WRONLY ...\n");
  int fd = open("/dev/null", O_WRONLY);
  SAY("  [1] open returned fd=%d errno=%d\n", fd, errno);
  if (fd < 0)
    FAIL("open_wronly", "got fd=%d errno=%d", fd, errno);
  else
    PASS("open_wronly");

  /* -----------------------------------------------------------------------
   * 2. write to /dev/null — must discard and return size
   * -------------------------------------------------------------------- */
  if (fd >= 0) {
    /* Write 1 byte first to rule out size-related issues */
    SAY("  [2a] write 1 byte ...\n");
    ssize_t n = write(fd, "x", 1);
    SAY("  [2a] write returned n=%zd errno=%d\n", n, errno);
    if (n != 1)
      FAIL("write_1byte", "expected 1, got %zd errno=%d", n, errno);
    else
      PASS("write_1byte");

    /* Write via a stack buffer */
    SAY("  [2b] write stack buf 5 bytes ...\n");
    char stackbuf[8] = "hello";
    n = write(fd, stackbuf, 5);
    SAY("  [2b] write returned n=%zd errno=%d\n", n, errno);
    if (n != 5)
      FAIL("write_stack", "expected 5, got %zd errno=%d", n, errno);
    else
      PASS("write_stack");

    /* Write a larger buffer */
    SAY("  [2c] write 64 bytes ...\n");
    static char bigbuf[64];
    memset(bigbuf, 'A', sizeof(bigbuf));
    n = write(fd, bigbuf, sizeof(bigbuf));
    SAY("  [2c] write returned n=%zd errno=%d\n", n, errno);
    if (n != 64)
      FAIL("write_64", "expected 64, got %zd errno=%d", n, errno);
    else
      PASS("write_64");

    close(fd);
    SAY("  [2d] close done\n");
  }

  /* -----------------------------------------------------------------------
   * 3. open /dev/null O_RDONLY — must return valid fd
   * -------------------------------------------------------------------- */
  SAY("  [3] open O_RDONLY ...\n");
  fd = open("/dev/null", O_RDONLY);
  SAY("  [3] open returned fd=%d errno=%d\n", fd, errno);
  if (fd < 0)
    FAIL("open_rdonly", "got fd=%d errno=%d", fd, errno);
  else
    PASS("open_rdonly");

  /* -----------------------------------------------------------------------
   * 4. read from /dev/null — must return 0 (EOF) immediately
   * -------------------------------------------------------------------- */
  if (fd >= 0) {
    SAY("  [4] read ...\n");
    char buf[16];
    ssize_t n = read(fd, buf, sizeof(buf));
    SAY("  [4] read returned n=%zd errno=%d\n", n, errno);
    if (n != 0)
      FAIL("read_devnull", "expected 0 (EOF), got %zd errno=%d", n, errno);
    else
      PASS("read_devnull");
    close(fd);
  }

  /* -----------------------------------------------------------------------
   * 5. open /dev/null O_WRONLY|O_CREAT|O_TRUNC (what freopen("w") does)
   * -------------------------------------------------------------------- */
  SAY("  [5] open O_WRONLY|O_CREAT|O_TRUNC ...\n");
  errno = 0;
  fd = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0666);
  SAY("  [5] open returned fd=%d errno=%d\n", fd, errno);
  if (fd < 0)
    FAIL("open_creat_trunc", "got fd=%d errno=%d", fd, errno);
  else
    PASS("open_creat_trunc");
  if (fd >= 0) close(fd);

  /* -----------------------------------------------------------------------
   * 6. freopen /dev/null onto stderr — must not hang, must return non-NULL
   * -------------------------------------------------------------------- */
  SAY("  [6] freopen(\"/dev/null\", \"w\", stderr) ...\n");
  errno = 0;
  FILE *f = freopen("/dev/null", "w", stderr);
  /* If freopen hangs we never reach here — that's the bug */
  SAY("  [6] freopen returned %p errno=%d\n", (void *)f, errno);
  if (!f)
    FAIL("freopen_stderr", "returned NULL errno=%d", errno);
  else
    PASS("freopen_stderr");

  /* -----------------------------------------------------------------------
   * 7. fprintf to the reopened stderr — must not hang
   * -------------------------------------------------------------------- */
  if (f) {
    SAY("  [7] fprintf to /dev/null stderr ...\n");
    int r = fprintf(stderr, "this goes to /dev/null\n");
    SAY("  [7] fprintf returned r=%d errno=%d\n", r, errno);
    if (r < 0)
      FAIL("fprintf_devnull", "returned %d errno=%d", r, errno);
    else
      PASS("fprintf_devnull");
  }

  /* -----------------------------------------------------------------------
   * 8. psignal — the original failing test (writes to stderr = /dev/null)
   * -------------------------------------------------------------------- */
  if (f) {
    SAY("  [8] psignal(SIGUSR1, \"foo\") with stderr=/dev/null ...\n");
    psignal(SIGUSR1, "foo");
    SAY("  [8] psignal returned\n");
    if (ferror(stderr))
      FAIL("psignal_devnull", "ferror set after psignal");
    else
      PASS("psignal_devnull");
  }

  SAY("\ntest_devnull: %d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
