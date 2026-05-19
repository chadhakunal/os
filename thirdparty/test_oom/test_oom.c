#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

static int passed = 0;
static int failed = 0;

#define SAY(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)
#define PASS(t) do { printf("  PASS: %s\n", (t)); fflush(stdout); passed++; } while(0)
#define FAIL(t, ...) do { printf("  FAIL: %s -- ", (t)); printf(__VA_ARGS__); printf("\n"); fflush(stdout); failed++; } while(0)

/* Read free memory in bytes from /proc/meminfo (MemFree line). */
static long read_mem_free_kb(void) {
  FILE *f = fopen("/proc/meminfo", "r");
  if (!f) return -1;
  char line[128];
  long kb = -1;
  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, "MemFree:", 8) == 0) {
      sscanf(line + 8, " %ld", &kb);
      break;
    }
  }
  fclose(f);
  return kb;
}

/*
 * OOM via anonymous page fault: child mmap's a huge lazy region and then
 * touches every page, exhausting physical memory. The kernel should kill it
 * with SIGKILL — the parent must NOT see a kernel panic.
 */
static void test_oom_anon_fault(void) {
  SAY("  [1] OOM via anonymous page fault (child touches all of RAM)...\n");

  long free_kb = read_mem_free_kb();
  if (free_kb < 0) {
    FAIL("oom_anon_fault", "could not read /proc/meminfo");
    return;
  }

  /* Map 2x free memory so we're guaranteed to exhaust it when touched. */
  size_t map_bytes = (size_t)(free_kb * 1024) * 2;

  pid_t pid = fork();
  if (pid < 0) { FAIL("oom_anon_fault", "fork failed"); return; }

  if (pid == 0) {
    /* Child: map a huge region then touch every page. */
    volatile char *p = mmap(NULL, map_bytes, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == (void *)-1) _exit(1);
    size_t page_size = 4096;
    for (size_t i = 0; i < map_bytes; i += page_size)
      p[i] = (char)i; /* trigger page fault for each page */
    _exit(0); /* should never reach here */
  }

  int status = 0;
  waitpid(pid, &status, 0);

  /* Kernel must have killed the child, not panicked.
   * It exits with code 128+SIGKILL=137 (our signal exit encoding). */
  int killed_by_oom = WIFEXITED(status) && WEXITSTATUS(status) == 128 + SIGKILL;
  if (killed_by_oom)
    PASS("oom_anon_fault");
  else
    FAIL("oom_anon_fault", "child status=0x%x exit=%d termsig=%d",
         status, WEXITSTATUS(status), WTERMSIG(status));
}

/*
 * OOM via COW fault: parent maps a page, forks, then child writes to trigger
 * COW while memory is exhausted by a concurrent large mapping.
 */
static void test_oom_cow_fault(void) {
  SAY("  [2] OOM via COW fault...\n");

  long free_kb = read_mem_free_kb();
  if (free_kb < 0) { FAIL("oom_cow_fault", "could not read /proc/meminfo"); return; }

  /* Allocate a shared page before forking. */
  volatile char *shared = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (shared == (void *)-1) { FAIL("oom_cow_fault", "initial mmap failed"); return; }
  shared[0] = 42;

  pid_t pid = fork();
  if (pid < 0) { munmap((void *)shared, 4096); FAIL("oom_cow_fault", "fork failed"); return; }

  if (pid == 0) {
    /* Child: first exhaust memory with a large lazy map that we touch,
     * then trigger a COW fault on the shared page. */
    size_t map_bytes = (size_t)(free_kb * 1024) * 2;
    volatile char *hog = mmap(NULL, map_bytes, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (hog != (void *)-1) {
      size_t page_size = 4096;
      for (size_t i = 0; i < map_bytes; i += page_size)
        hog[i] = 1;
    }
    /* Now try to COW-fault the shared page — should OOM-kill us. */
    shared[0] = 99;
    _exit(0);
  }

  munmap((void *)shared, 4096);

  int status = 0;
  waitpid(pid, &status, 0);

  int killed_by_oom = WIFEXITED(status) && WEXITSTATUS(status) == 128 + SIGKILL;
  if (killed_by_oom)
    PASS("oom_cow_fault");
  else
    FAIL("oom_cow_fault", "child status=0x%x exit=%d termsig=%d",
         status, WEXITSTATUS(status), WTERMSIG(status));
}

/*
 * OOM write bounce buffer: child exhausts memory then tries a write syscall.
 * write() should return ENOMEM, not panic.
 */
static void test_oom_write_enomem(void) {
  SAY("  [3] OOM write syscall returns ENOMEM (not panic)...\n");

  long free_kb = read_mem_free_kb();
  if (free_kb < 0) { FAIL("oom_write_enomem", "could not read /proc/meminfo"); return; }

  pid_t pid = fork();
  if (pid < 0) { FAIL("oom_write_enomem", "fork failed"); return; }

  if (pid == 0) {
    /* Exhaust memory. */
    size_t map_bytes = (size_t)(free_kb * 1024) * 2;
    volatile char *hog = mmap(NULL, map_bytes, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (hog != (void *)-1) {
      size_t page_size = 4096;
      for (size_t i = 0; i < map_bytes; i += page_size)
        hog[i] = 1;
    }
    /* Try to write — kernel needs a bounce page. Should get ENOMEM. */
    char buf[] = "x";
    ssize_t n = write(1, buf, 1);
    /* Either ENOMEM or success (if write grabbed a page before hog) — either
     * way, not a panic. Exit 0 to signal the write syscall returned at all. */
    (void)n;
    _exit(0);
  }

  int status = 0;
  waitpid(pid, &status, 0);

  /* Success means the child exited normally (write returned, kernel didn't panic).
   * It may have been OOM-killed by the mmap touching, which is also fine. */
  int no_panic = WIFEXITED(status) &&
                 (WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == 128 + SIGKILL);
  if (no_panic)
    PASS("oom_write_enomem");
  else
    FAIL("oom_write_enomem", "child status=0x%x exit=%d",
         status, WEXITSTATUS(status));
}

/*
 * Kernel stays alive after OOM kills: after the OOM child is gone, the parent
 * can still allocate and use memory normally.
 */
static void test_kernel_survives_oom(void) {
  SAY("  [4] kernel survives OOM — normal allocation works after...\n");

  long free_kb = read_mem_free_kb();
  if (free_kb < 0) { FAIL("kernel_survives_oom", "could not read /proc/meminfo"); return; }

  /* Spawn and reap an OOM child. */
  pid_t pid = fork();
  if (pid == 0) {
    size_t map_bytes = (size_t)(free_kb * 1024) * 2;
    volatile char *p = mmap(NULL, map_bytes, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p != (void *)-1) {
      for (size_t i = 0; i < map_bytes; i += 4096) p[i] = 1;
    }
    _exit(0);
  }
  int status;
  waitpid(pid, &status, 0);

  /* Now verify we can still allocate and use a page normally. */
  void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  int ok = (p != (void *)-1 && p != NULL);
  if (ok) {
    volatile char *c = (volatile char *)p;
    c[0] = 0x42;
    ok = (c[0] == 0x42);
    munmap(p, 4096);
  }

  if (ok)
    PASS("kernel_survives_oom");
  else
    FAIL("kernel_survives_oom", "post-OOM mmap failed");
}

int main(void) {
  SAY("test_oom: start\n");

  test_oom_anon_fault();
  test_oom_cow_fault();
  test_oom_write_enomem();
  test_kernel_survives_oom();

  SAY("\ntest_oom: %d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
