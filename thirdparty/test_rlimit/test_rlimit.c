/*
 * RLIMIT tests for sbunix: NOFILE, AS, FSIZE.
 *
 * Run: /bin/test_rlimit
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/resource.h>

#define PAGE_SIZE 4096
#define MAP_ANON_PRIVATE (MAP_PRIVATE | MAP_ANONYMOUS)

static int passed;
static int failed;
static int skipped;

static void pass(const char *name) {
  printf("  [PASS] %s\n", name);
  passed++;
}

static void fail(const char *name) {
  printf("  [FAIL] %s\n", name);
  failed++;
}

static void skip(const char *name, const char *why) {
  printf("  [SKIP] %s (%s)\n", name, why);
  skipped++;
}

static int require_rlimit_syscalls(void) {
  struct rlimit rl;
  if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
    return 1;
  printf("SKIP: getrlimit unavailable (errno=%d)\n", errno);
  return 0;
}

/* --- RLIMIT_NOFILE --- */

static void test_getrlimit_nofile_ok(void) {
  struct rlimit rl;
  if (getrlimit(RLIMIT_NOFILE, &rl) == 0)
    pass("getrlimit(RLIMIT_NOFILE) succeeds");
  else
    fail("getrlimit(RLIMIT_NOFILE) succeeds");
}

static void test_getrlimit_as_ok(void) {
  struct rlimit rl;
  if (getrlimit(RLIMIT_AS, &rl) == 0)
    pass("getrlimit(RLIMIT_AS) succeeds");
  else
    fail("getrlimit(RLIMIT_AS) succeeds");
}

static void test_getrlimit_fsize_ok(void) {
  struct rlimit rl;
  if (getrlimit(RLIMIT_FSIZE, &rl) == 0)
    pass("getrlimit(RLIMIT_FSIZE) succeeds");
  else
    fail("getrlimit(RLIMIT_FSIZE) succeeds");
}

static void test_soft_le_hard(int resource, const char *name) {
  struct rlimit rl;
  if (getrlimit(resource, &rl) != 0) {
    fail(name);
    return;
  }
  if (rl.rlim_cur == RLIM_INFINITY || rl.rlim_max == RLIM_INFINITY) {
    pass(name);
    return;
  }
  if (rl.rlim_cur <= rl.rlim_max)
    pass(name);
  else
    fail(name);
}

#define STD_FD_COUNT 3

static void test_nofile_enforcement(void) {
  struct rlimit saved, low;
  const int extra_allowed = 3;
  int fd;
  int opened = 0;
  int fds[32];
  int got_emfile = 0;

  if (getrlimit(RLIMIT_NOFILE, &saved) != 0) {
    fail("open fails with EMFILE at soft NOFILE limit");
    return;
  }

  low = saved;
  low.rlim_cur = STD_FD_COUNT + extra_allowed;
  if (low.rlim_cur > saved.rlim_max)
    low.rlim_cur = saved.rlim_max;
  if (low.rlim_cur <= STD_FD_COUNT) {
    skip("open fails with EMFILE at soft NOFILE limit", "soft limit too low");
    return;
  }

  if (setrlimit(RLIMIT_NOFILE, &low) != 0) {
    fail("open fails with EMFILE at soft NOFILE limit");
    return;
  }

  while (opened < (int)(sizeof(fds) / sizeof(fds[0]))) {
    fd = open("/etc/rc", O_RDONLY);
    if (fd < 0) {
      if (errno == EMFILE) {
        got_emfile = 1;
        break;
      }
      fail("open fails with EMFILE at soft NOFILE limit");
      goto cleanup;
    }
    fds[opened++] = fd;
  }

  if (got_emfile && opened == extra_allowed)
    pass("open fails with EMFILE at soft NOFILE limit");
  else
    fail("open fails with EMFILE at soft NOFILE limit");

cleanup:
  for (int i = 0; i < opened; i++)
    close(fds[i]);
  setrlimit(RLIMIT_NOFILE, &saved);
}

/* --- RLIMIT_AS --- */

static void test_as_enforcement(void) {
  struct rlimit saved, low;
  void *p;

  if (getrlimit(RLIMIT_AS, &saved) != 0) {
    fail("mmap fails with ENOMEM at soft AS limit");
    return;
  }

  /*
   * This process already has text/stack/heap VMAs >> one page. Lower soft AS to
   * one page; any new mmap must fail with ENOMEM.
   */
  low = saved;
  low.rlim_cur = PAGE_SIZE;
  if (low.rlim_max != RLIM_INFINITY && low.rlim_max < low.rlim_cur)
    low.rlim_max = low.rlim_cur;

  if (setrlimit(RLIMIT_AS, &low) != 0) {
    fail("mmap fails with ENOMEM at soft AS limit");
    return;
  }

  p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON_PRIVATE, -1, 0);
  if (p == (void *)-1 && errno == ENOMEM)
    pass("mmap fails with ENOMEM at soft AS limit");
  else {
    if (p != (void *)-1)
      munmap(p, PAGE_SIZE);
    fail("mmap fails with ENOMEM at soft AS limit");
  }

  setrlimit(RLIMIT_AS, &saved);
}

/* --- RLIMIT_FSIZE --- */

static void test_fsize_enforcement(void) {
  struct rlimit saved, low;
  struct sigaction sa, old;
  const char *path = "/mnt/tmp/rlimit_fsize_test";
  char buf[64];
  int fd;
  ssize_t n;

  if (getrlimit(RLIMIT_FSIZE, &saved) != 0) {
    fail("write fails with EFBIG at soft FSIZE limit");
    return;
  }

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGXFSZ, &sa, &old);

  low = saved;
  if (low.rlim_max == RLIM_INFINITY)
    low.rlim_max = 4096;
  low.rlim_cur = 64;
  if (low.rlim_cur > low.rlim_max)
    low.rlim_cur = low.rlim_max;

  if (setrlimit(RLIMIT_FSIZE, &low) != 0) {
    fail("write fails with EFBIG at soft FSIZE limit");
    goto restore_sig;
  }

  fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (fd < 0) {
    skip("write fails with EFBIG at soft FSIZE limit", "need writable filesystem");
    goto restore_rlimit;
  }

  memset(buf, 'x', sizeof(buf));
  if (write(fd, buf, 64) != 64) {
    fail("write fails with EFBIG at soft FSIZE limit");
    goto cleanup_file;
  }

  n = write(fd, buf, 1);
  if (n < 0 && errno == EFBIG)
    pass("write fails with EFBIG at soft FSIZE limit");
  else
    fail("write fails with EFBIG at soft FSIZE limit");

cleanup_file:
  close(fd);
  unlink(path);
restore_rlimit:
  setrlimit(RLIMIT_FSIZE, &saved);
restore_sig:
  sigaction(SIGXFSZ, &old, NULL);
}

int main(void) {
  printf("=== RLIMIT tests ===\n\n");

  if (!require_rlimit_syscalls()) {
    printf("\n0 passed, 0 failed (syscalls not implemented)\n");
    return 0;
  }

  printf("--- NOFILE ---\n");
  test_getrlimit_nofile_ok();
  test_soft_le_hard(RLIMIT_NOFILE, "NOFILE soft limit <= hard limit");
  test_nofile_enforcement();

  printf("\n--- AS ---\n");
  test_getrlimit_as_ok();
  test_soft_le_hard(RLIMIT_AS, "AS soft limit <= hard limit");
  test_as_enforcement();

  printf("\n--- FSIZE ---\n");
  test_getrlimit_fsize_ok();
  test_soft_le_hard(RLIMIT_FSIZE, "FSIZE soft limit <= hard limit");
  test_fsize_enforcement();

  printf("\n%d passed, %d failed, %d skipped\n", passed, failed, skipped);
  return failed ? 1 : 0;
}
