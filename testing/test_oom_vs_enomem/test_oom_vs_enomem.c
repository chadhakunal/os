/*
 * test_oom_vs_enomem: verify the kernel distinguishes between two very
 * different "out of memory" conditions:
 *
 *   ENOMEM  — virtual address space exhausted (RLIMIT_AS enforced at mmap
 *              time).  mmap returns MAP_FAILED with errno=ENOMEM.  The
 *              process is NOT killed; it can recover and keep running.
 *
 *   SIGKILL — physical pages exhausted.  The page-fault handler cannot
 *              satisfy the fault, calls oom_kill_current(), and the process
 *              is killed by SIGKILL before returning to user space.
 *
 * QEMU virt machine has 128 MB of RAM.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define PAGE_SIZE       4096
#define MAP_ANON_PRIVATE (MAP_PRIVATE | MAP_ANONYMOUS)

/* 256 MB — always exceeds the 128 MB physical RAM in QEMU virt. */
#define PHYS_EXHAUST_BYTES (256UL * 1024 * 1024)

static int passed;
static int failed;

static void pass(const char *name) { printf("  PASS: %s\n", name); fflush(stdout); passed++; }
static void fail(const char *name, const char *msg) {
  printf("  FAIL: %s -- %s\n", name, msg); fflush(stdout); failed++;
}

/* ------------------------------------------------------------------ */
/* helpers                                                              */
/* ------------------------------------------------------------------ */

static void touch_pages(volatile char *p, size_t len) {
  for (size_t i = 0; i < len; i += PAGE_SIZE)
    p[i] = (char)i;
}

/*
 * Lower RLIMIT_AS soft limit to `cap` bytes (hard limit unchanged).
 * Returns 0 on success, -1 and prints a message on failure.
 */
static int set_as_soft(uint64_t cap, struct rlimit *saved) {
  if (getrlimit(RLIMIT_AS, saved) != 0) {
    printf("  SKIP: getrlimit(RLIMIT_AS) failed (errno=%d)\n", errno);
    return -1;
  }
  struct rlimit low = *saved;
  low.rlim_cur = cap;
  if (setrlimit(RLIMIT_AS, &low) != 0) {
    printf("  SKIP: setrlimit(RLIMIT_AS) failed (errno=%d)\n", errno);
    return -1;
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* [1] ENOMEM: mmap rejected by RLIMIT_AS — process survives           */
/* ------------------------------------------------------------------ */
static void test_enomem_mmap_rejected(void) {
  printf("  [1] RLIMIT_AS: mmap returns ENOMEM, process survives...\n");
  const char *name = "enomem_mmap_rejected";

  struct rlimit saved;
  /*
   * Set AS soft limit to 1 page.  The process already has text/stack VMAs,
   * so any new mmap must exceed the limit and return ENOMEM.
   */
  if (set_as_soft(PAGE_SIZE, &saved) != 0) return;

  void *p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON_PRIVATE, -1, 0);
  int got_enomem = (p == (void *)-1 && errno == ENOMEM);

  if (p != (void *)-1) munmap(p, PAGE_SIZE);
  setrlimit(RLIMIT_AS, &saved);

  if (got_enomem)
    pass(name);
  else
    fail(name, "expected mmap to fail with ENOMEM");
}

/* ------------------------------------------------------------------ */
/* [2] ENOMEM: process continues after a rejected mmap                 */
/* ------------------------------------------------------------------ */
static void test_enomem_process_continues(void) {
  printf("  [2] RLIMIT_AS: process keeps running after ENOMEM from mmap...\n");
  const char *name = "enomem_process_continues";

  struct rlimit saved;
  if (set_as_soft(PAGE_SIZE, &saved) != 0) return;

  void *p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON_PRIVATE, -1, 0);
  int mmap_failed_correctly = (p == (void *)-1 && errno == ENOMEM);

  if (p != (void *)-1) munmap(p, PAGE_SIZE);
  setrlimit(RLIMIT_AS, &saved);

  /*
   * After restoring the limit the process must still be able to allocate.
   * If we're here we haven't been killed — that alone proves survival.
   * Do a small allocation to confirm the allocator still works.
   */
  void *check = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON_PRIVATE, -1, 0);
  int can_alloc_after = (check != (void *)-1);
  if (can_alloc_after) {
    volatile char *c = (volatile char *)check;
    c[0] = 0xAB;
    can_alloc_after = (c[0] == 0xAB);
    munmap(check, PAGE_SIZE);
  }

  if (mmap_failed_correctly && can_alloc_after)
    pass(name);
  else if (!mmap_failed_correctly)
    fail(name, "mmap did not return ENOMEM as expected");
  else
    fail(name, "process could not allocate memory after restoring limit");
}

/* ------------------------------------------------------------------ */
/* [3] OOM kill: physical exhaustion → SIGKILL, not ENOMEM             */
/* ------------------------------------------------------------------ */
static void test_oom_kill_not_enomem(void) {
  printf("  [3] physical OOM: child killed by SIGKILL, not ENOMEM...\n");
  const char *name = "oom_kill_not_enomem";

  pid_t pid = fork();
  if (pid < 0) { fail(name, "fork failed"); return; }

  if (pid == 0) {
    /*
     * No RLIMIT_AS constraint — mmap succeeds (virtual reservation is free).
     * Touching pages forces physical allocation until RAM is exhausted.
     * The kernel OOM-kills us via SIGKILL during touch_pages.
     */
    volatile char *p = mmap(NULL, PHYS_EXHAUST_BYTES, PROT_READ | PROT_WRITE,
                            MAP_ANON_PRIVATE, -1, 0);
    if (p == (void *)-1) _exit(42); /* mmap itself must not fail */
    touch_pages(p, PHYS_EXHAUST_BYTES);
    _exit(0); /* unreachable if OOM kill works */
  }

  int status = 0;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status) && WEXITSTATUS(status) == 42)
    fail(name, "mmap failed (ENOMEM) even without RLIMIT_AS — OOM killer never ran");
  else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL)
    pass(name);
  else
    fail(name, "unexpected exit status");
}

/* ------------------------------------------------------------------ */
/* [4] ENOMEM vs OOM are independent: RLIMIT_AS ENOMEM in parent,      */
/*     then child hits physical OOM and is killed                       */
/* ------------------------------------------------------------------ */
static void test_enomem_then_child_oom(void) {
  printf("  [4] ENOMEM in parent does not prevent child OOM kill...\n");
  const char *name = "enomem_then_child_oom";

  /* Parent: get ENOMEM from RLIMIT_AS. */
  struct rlimit saved;
  if (set_as_soft(PAGE_SIZE, &saved) != 0) return;

  void *p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON_PRIVATE, -1, 0);
  int parent_got_enomem = (p == (void *)-1 && errno == ENOMEM);
  if (p != (void *)-1) munmap(p, PAGE_SIZE);
  setrlimit(RLIMIT_AS, &saved);

  if (!parent_got_enomem) { fail(name, "parent did not get ENOMEM"); return; }

  /* Child: exhaust physical RAM — must be OOM-killed. */
  pid_t pid = fork();
  if (pid < 0) { fail(name, "fork failed"); return; }

  if (pid == 0) {
    volatile char *q = mmap(NULL, PHYS_EXHAUST_BYTES, PROT_READ | PROT_WRITE,
                            MAP_ANON_PRIVATE, -1, 0);
    if (q == (void *)-1) _exit(42);
    touch_pages(q, PHYS_EXHAUST_BYTES);
    _exit(0);
  }

  int status = 0;
  waitpid(pid, &status, 0);

  if (WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL)
    pass(name);
  else if (WIFEXITED(status) && WEXITSTATUS(status) == 42)
    fail(name, "child mmap failed (ENOMEM) — RLIMIT_AS inherited incorrectly?");
  else
    fail(name, "child not killed by SIGKILL");
}

/* ------------------------------------------------------------------ */
/* [5] RLIMIT_AS raised back to unlimited — large mmap succeeds         */
/* ------------------------------------------------------------------ */
static void test_raise_limit_allows_large_mmap(void) {
  printf("  [5] raising RLIMIT_AS to unlimited allows large mmap...\n");
  const char *name = "raise_limit_allows_large_mmap";

  struct rlimit saved;
  if (getrlimit(RLIMIT_AS, &saved) != 0) { fail(name, "getrlimit failed"); return; }

  /* Lower to 1 page, confirm rejection, then raise back. */
  struct rlimit low = saved;
  low.rlim_cur = PAGE_SIZE;
  if (setrlimit(RLIMIT_AS, &low) != 0) { fail(name, "setrlimit low failed"); return; }

  void *p = mmap(NULL, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON_PRIVATE, -1, 0);
  int rejected = (p == (void *)-1 && errno == ENOMEM);
  if (p != (void *)-1) munmap(p, 2 * PAGE_SIZE);

  /* Restore unlimited. */
  if (setrlimit(RLIMIT_AS, &saved) != 0) { fail(name, "setrlimit restore failed"); return; }

  /* Now the same mmap must succeed. */
  p = mmap(NULL, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON_PRIVATE, -1, 0);
  int allowed = (p != (void *)-1);
  if (allowed) munmap(p, 2 * PAGE_SIZE);

  if (rejected && allowed)
    pass(name);
  else if (!rejected)
    fail(name, "mmap was not rejected at low limit");
  else
    fail(name, "mmap still rejected after restoring limit");
}

/* ------------------------------------------------------------------ */
/* [6] setrlimit cannot raise soft above hard                           */
/* ------------------------------------------------------------------ */
static void test_cannot_raise_soft_above_hard(void) {
  printf("  [6] setrlimit cannot raise soft limit above hard limit...\n");
  const char *name = "cannot_raise_soft_above_hard";

  struct rlimit saved;
  if (getrlimit(RLIMIT_NOFILE, &saved) != 0) { fail(name, "getrlimit failed"); return; }

  if (saved.rlim_max == RLIM_INFINITY) {
    /* Can't test this with an infinite hard limit — skip gracefully. */
    printf("  SKIP: %s (hard limit is RLIM_INFINITY)\n", name);
    return;
  }

  struct rlimit bad = saved;
  bad.rlim_cur = saved.rlim_max + 1; /* soft > hard */
  int ret = setrlimit(RLIMIT_NOFILE, &bad);

  if (ret == -1 && errno == EINVAL)
    pass(name);
  else
    fail(name, "expected setrlimit to fail with EINVAL");
}

/* ------------------------------------------------------------------ */

int main(void) {
  printf("test_oom_vs_enomem: start\n\n");

  test_enomem_mmap_rejected();
  test_enomem_process_continues();
  test_oom_kill_not_enomem();
  test_enomem_then_child_oom();
  test_raise_limit_allows_large_mmap();
  test_cannot_raise_soft_above_hard();

  printf("\ntest_oom_vs_enomem: %d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
