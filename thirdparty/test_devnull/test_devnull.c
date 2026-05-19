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

#define PASS(t) do { printf("  PASS: %s\n", (t)); passed++; } while(0)
#define FAIL(t, ...) do { printf("  FAIL: %s — ", (t)); printf(__VA_ARGS__); printf("\n"); failed++; } while(0)

int main(void) {
  printf("test_devnull: start\n");

  /* -----------------------------------------------------------------------
   * 1. open /dev/null O_WRONLY — must not hang, must return valid fd
   * -------------------------------------------------------------------- */
  printf("  [1] open O_WRONLY ...\n");
  int fd = open("/dev/null", O_WRONLY);
  printf("  [1] open returned fd=%d errno=%d\n", fd, errno);
  if (fd < 0)
    FAIL("open_wronly", "got fd=%d errno=%d", fd, errno);
  else
    PASS("open_wronly");

  /* -----------------------------------------------------------------------
   * 2. write to /dev/null — must discard and return size
   * -------------------------------------------------------------------- */
  if (fd >= 0) {
    printf("  [2] write ...\n");
    ssize_t n = write(fd, "hello", 5);
    printf("  [2] write returned n=%zd errno=%d\n", n, errno);
    if (n != 5)
      FAIL("write_devnull", "expected 5, got %zd errno=%d", n, errno);
    else
      PASS("write_devnull");
    close(fd);
  }

  /* -----------------------------------------------------------------------
   * 3. open /dev/null O_RDONLY — must return valid fd
   * -------------------------------------------------------------------- */
  printf("  [3] open O_RDONLY ...\n");
  fd = open("/dev/null", O_RDONLY);
  printf("  [3] open returned fd=%d errno=%d\n", fd, errno);
  if (fd < 0)
    FAIL("open_rdonly", "got fd=%d errno=%d", fd, errno);
  else
    PASS("open_rdonly");

  /* -----------------------------------------------------------------------
   * 4. read from /dev/null — must return 0 (EOF) immediately
   * -------------------------------------------------------------------- */
  if (fd >= 0) {
    printf("  [4] read ...\n");
    char buf[16];
    ssize_t n = read(fd, buf, sizeof(buf));
    printf("  [4] read returned n=%zd errno=%d\n", n, errno);
    if (n != 0)
      FAIL("read_devnull", "expected 0 (EOF), got %zd errno=%d", n, errno);
    else
      PASS("read_devnull");
    close(fd);
  }

  /* -----------------------------------------------------------------------
   * 5. open /dev/null O_WRONLY|O_CREAT|O_TRUNC (what freopen("w") does)
   * -------------------------------------------------------------------- */
  printf("  [5] open O_WRONLY|O_CREAT|O_TRUNC ...\n");
  errno = 0;
  fd = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0666);
  printf("  [5] open returned fd=%d errno=%d\n", fd, errno);
  if (fd < 0)
    FAIL("open_creat_trunc", "got fd=%d errno=%d", fd, errno);
  else
    PASS("open_creat_trunc");
  if (fd >= 0) close(fd);

  /* -----------------------------------------------------------------------
   * 6. freopen /dev/null onto stderr — must not hang, must return non-NULL
   * -------------------------------------------------------------------- */
  printf("  [6] freopen(\"/dev/null\", \"w\", stderr) ...\n");
  errno = 0;
  FILE *f = freopen("/dev/null", "w", stderr);
  /* If freopen hangs we never reach here — that's the bug */
  printf("  [6] freopen returned %p errno=%d\n", (void *)f, errno);
  if (!f)
    FAIL("freopen_stderr", "returned NULL errno=%d", errno);
  else
    PASS("freopen_stderr");

  /* -----------------------------------------------------------------------
   * 7. fprintf to the reopened stderr — must not hang
   * -------------------------------------------------------------------- */
  if (f) {
    printf("  [7] fprintf to /dev/null stderr ...\n");
    int r = fprintf(stderr, "this goes to /dev/null\n");
    printf("  [7] fprintf returned r=%d errno=%d\n", r, errno);
    if (r < 0)
      FAIL("fprintf_devnull", "returned %d errno=%d", r, errno);
    else
      PASS("fprintf_devnull");
  }

  /* -----------------------------------------------------------------------
   * 8. psignal — the original failing test (writes to stderr = /dev/null)
   * -------------------------------------------------------------------- */
  if (f) {
    printf("  [8] psignal(SIGUSR1, \"foo\") with stderr=/dev/null ...\n");
    psignal(SIGUSR1, "foo");
    printf("  [8] psignal returned\n");
    if (ferror(stderr))
      FAIL("psignal_devnull", "ferror set after psignal");
    else
      PASS("psignal_devnull");
  }

  printf("\ntest_devnull: %d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
