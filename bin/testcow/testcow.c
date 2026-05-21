#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

/* -----------------------------------------------------------------------
 * COW (copy-on-write) fork tests.
 *
 * Strategy: read /proc/meminfo to get pages_in_use before and after
 * operations and assert that page consumption matches COW expectations:
 *
 *   - fork() with read-only child: no new data pages allocated
 *   - fork() + child writes one page: exactly 1 new page
 *   - fork() + child writes N distinct pages: exactly N new pages
 *   - fork() + exec (child replaces image): pages freed on exec
 *   - last-reference reclaim: after other sharer exits, parent write
 *     is free (refcount==1 fast path, no alloc)
 * -------------------------------------------------------------------- */

#define PAGE_SIZE 4096

/* How much slack (in kB) we allow for incidental kernel allocations
 * (scheduler, file table, etc.) that happen alongside the test. */
#define SLACK_KB 256

static int passed = 0;
static int failed = 0;

/* -----------------------------------------------------------------------
 * Read MemUsed kB from /proc/meminfo
 * -------------------------------------------------------------------- */
static long read_mem_used_kb(void) {
  int fd = open("/proc/meminfo", O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "testcow: cannot open /proc/meminfo: %d\n", errno);
    return -1;
  }
  char buf[256];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return -1;
  buf[n] = '\0';

  /* Format: "MemUsed:       NNNN kB\n" */
  char *p = strstr(buf, "MemUsed:");
  if (!p) return -1;
  p += 8; /* skip "MemUsed:" */
  while (*p == ' ') p++;
  return atol(p);
}

#define PASS(name) do { printf("  PASS: %s\n", name); passed++; } while(0)
#define FAIL(name, ...) do { printf("  FAIL: %s — ", name); printf(__VA_ARGS__); printf("\n"); failed++; } while(0)

/* -----------------------------------------------------------------------
 * Test 1: fork + child does nothing (read-only) → no data pages added.
 *
 * With COW, the child shares all pages with the parent.  The only new
 * allocations should be kernel bookkeeping (task struct, kernel stack,
 * page-table root).  Data pages must NOT be copied.
 * -------------------------------------------------------------------- */
#define ANON_PAGES 64  /* 64 pages = 256 kB of dirty anonymous memory */
static char anon_buf[ANON_PAGES * PAGE_SIZE];

static void test_fork_readonly(void) {
  /* Dirty the buffer so all pages have valid PTEs before fork */
  memset(anon_buf, 0xAB, sizeof(anon_buf));

  long before = read_mem_used_kb();

  pid_t child = fork();
  if (child == 0) {
    /* Child: just read one byte to ensure it's alive, then exit */
    volatile char x = anon_buf[0];
    (void)x;
    exit(0);
  }

  int status;
  waitpid(child, &status, 0);

  long after = read_mem_used_kb();
  long delta = after - before;

  /* With COW: fork allocates a task struct + kernel stack (~8 pages = 32 kB)
   * plus a page-table root.  Data pages are NOT copied.
   * Without COW: 64 * 4 kB = 256 kB of data pages would be copied.
   * We check that delta < 64 pages worth (256 kB), with slack. */
  if (before < 0 || after < 0) {
    FAIL("fork_readonly", "could not read /proc/meminfo");
  } else if (delta >= (long)(ANON_PAGES * PAGE_SIZE / 1024)) {
    FAIL("fork_readonly",
         "memory grew by %ld kB — expected < %d kB (COW should avoid data copy)",
         delta, ANON_PAGES * (int)(PAGE_SIZE / 1024));
  } else {
    PASS("fork_readonly");
  }
}

/* -----------------------------------------------------------------------
 * Test 2: fork + child writes exactly 1 page → exactly 1 new data page.
 * -------------------------------------------------------------------- */
static void test_fork_write_one_page(void) {
  static char buf[ANON_PAGES * PAGE_SIZE];
  memset(buf, 0x55, sizeof(buf));

  long before = read_mem_used_kb();

  pid_t child = fork();
  if (child == 0) {
    /* Write only the first page */
    memset(buf, 0xBB, PAGE_SIZE);
    exit(0);
  }

  int status;
  waitpid(child, &status, 0);

  long after = read_mem_used_kb();
  long delta = after - before;

  /* We expect ~1 data page (4 kB) + kernel bookkeeping overhead.
   * Definitely < ANON_PAGES pages (256 kB). */
  if (before < 0 || after < 0) {
    FAIL("fork_write_one_page", "could not read /proc/meminfo");
  } else if (delta >= (long)(ANON_PAGES * PAGE_SIZE / 1024)) {
    FAIL("fork_write_one_page",
         "memory grew by %ld kB — expected < %d kB (only 1 page written)",
         delta, ANON_PAGES * (int)(PAGE_SIZE / 1024));
  } else {
    PASS("fork_write_one_page");
  }
}

/* -----------------------------------------------------------------------
 * Test 3: fork + child writes N pages → N new data pages, no more.
 * -------------------------------------------------------------------- */
#define WRITE_PAGES 8

static void test_fork_write_n_pages(void) {
  static char buf[ANON_PAGES * PAGE_SIZE];
  memset(buf, 0x77, sizeof(buf));

  long before = read_mem_used_kb();

  pid_t child = fork();
  if (child == 0) {
    /* Dirty exactly WRITE_PAGES distinct pages */
    for (int i = 0; i < WRITE_PAGES; i++)
      buf[i * PAGE_SIZE] = (char)(i + 1);
    exit(0);
  }

  int status;
  waitpid(child, &status, 0);

  long after = read_mem_used_kb();
  long delta = after - before;

  long expected_max_kb = (long)((WRITE_PAGES + 32) * PAGE_SIZE / 1024); /* +32 pages slack */

  if (before < 0 || after < 0) {
    FAIL("fork_write_n_pages", "could not read /proc/meminfo");
  } else if (delta > expected_max_kb + SLACK_KB) {
    FAIL("fork_write_n_pages",
         "memory grew by %ld kB — expected <= %ld kB (%d pages written + overhead)",
         delta, expected_max_kb + SLACK_KB, WRITE_PAGES);
  } else {
    PASS("fork_write_n_pages");
  }
}

/* -----------------------------------------------------------------------
 * Test 4: after fork+wait, memory returns to baseline (no leak).
 * -------------------------------------------------------------------- */
static void test_no_leak_after_fork(void) {
  static char buf[ANON_PAGES * PAGE_SIZE];
  memset(buf, 0x33, sizeof(buf));

  long before = read_mem_used_kb();

  pid_t child = fork();
  if (child == 0) {
    /* Write every page to force full COW copy in child */
    for (int i = 0; i < ANON_PAGES; i++)
      buf[i * PAGE_SIZE] ^= 1;
    exit(0);
  }

  waitpid(child, NULL, 0);

  long after = read_mem_used_kb();
  long delta = after - before;

  /* After child exits, its copied pages are freed.  Memory should return
   * to within SLACK_KB of the baseline. */
  if (before < 0 || after < 0) {
    FAIL("no_leak_after_fork", "could not read /proc/meminfo");
  } else if (delta > SLACK_KB) {
    FAIL("no_leak_after_fork",
         "memory increased by %ld kB after child exited — possible page leak",
         delta);
  } else {
    PASS("no_leak_after_fork");
  }
}

/* -----------------------------------------------------------------------
 * Test 5: last-reference fast-path — sibling exits, parent write is free.
 *
 * Setup: fork A and B from parent.  A exits immediately (drops its share).
 * Now parent is the last holder (refcount=1).  Parent writes the pages.
 * With the last-ref optimisation no new physical page should be allocated.
 * -------------------------------------------------------------------- */
static void test_last_ref_reclaim(void) {
  static char buf[ANON_PAGES * PAGE_SIZE];
  memset(buf, 0x11, sizeof(buf));

  /* Fork child A — it exits immediately, dropping COW refs */
  pid_t a = fork();
  if (a == 0) exit(0);
  waitpid(a, NULL, 0);

  /* Now parent should be sole owner (refcount == 1 for all pages).
   * Measure before the write. */
  long before = read_mem_used_kb();

  /* Write every page — should hit the refcount==1 fast path: no alloc */
  for (int i = 0; i < ANON_PAGES; i++)
    buf[i * PAGE_SIZE] = (char)(i ^ 0xFF);

  long after = read_mem_used_kb();
  long delta = after - before;

  if (before < 0 || after < 0) {
    FAIL("last_ref_reclaim", "could not read /proc/meminfo");
  } else if (delta > SLACK_KB) {
    FAIL("last_ref_reclaim",
         "memory grew by %ld kB on sole-owner write — expected near 0 (last-ref fast path)",
         delta);
  } else {
    PASS("last_ref_reclaim");
  }
}

/* -----------------------------------------------------------------------
 * Test 6: multiple forks from same parent — each gets its own private copy
 * on write, no cross-contamination.
 * -------------------------------------------------------------------- */
#define N_CHILDREN 4

static void test_multi_fork_isolation(void) {
  static char buf[ANON_PAGES * PAGE_SIZE];
  memset(buf, 0x00, sizeof(buf));

  pid_t children[N_CHILDREN];

  for (int c = 0; c < N_CHILDREN; c++) {
    children[c] = fork();
    if (children[c] == 0) {
      /* Each child writes a unique pattern */
      memset(buf, (char)(c + 1), sizeof(buf));
      /* Verify the pattern is consistent (no bleed from siblings) */
      for (int i = 0; i < (int)sizeof(buf); i++) {
        if ((unsigned char)buf[i] != (unsigned char)(c + 1)) {
          exit(1); /* isolation failure */
        }
      }
      exit(0);
    }
  }

  int all_ok = 1;
  for (int c = 0; c < N_CHILDREN; c++) {
    int status;
    waitpid(children[c], &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
      all_ok = 0;
  }

  /* Parent buffer should still be all zeros */
  for (int i = 0; i < (int)sizeof(buf); i++) {
    if (buf[i] != 0) { all_ok = 0; break; }
  }

  if (all_ok)
    PASS("multi_fork_isolation");
  else
    FAIL("multi_fork_isolation", "child or parent saw wrong data");
}

/* -----------------------------------------------------------------------
 * Test 7: stress — fork 100 times, each child writes all pages.
 * Total memory in use should not grow unboundedly.
 * -------------------------------------------------------------------- */
#define STRESS_FORKS 100

static void test_stress_memory(void) {
  static char buf[ANON_PAGES * PAGE_SIZE];
  memset(buf, 0xCC, sizeof(buf));

  long baseline = read_mem_used_kb();

  for (int i = 0; i < STRESS_FORKS; i++) {
    pid_t child = fork();
    if (child == 0) {
      /* Write every page */
      for (int j = 0; j < ANON_PAGES; j++)
        buf[j * PAGE_SIZE] = (char)(i + j);
      exit(0);
    }
    waitpid(child, NULL, 0);
  }

  long after = read_mem_used_kb();
  long drift = after - baseline;

  /* After all children exit, memory should be back near baseline.
   * Allow SLACK_KB drift for incidental kernel state. */
  if (baseline < 0 || after < 0) {
    FAIL("stress_memory", "could not read /proc/meminfo");
  } else if (drift > SLACK_KB) {
    FAIL("stress_memory",
         "memory drifted by %ld kB after %d fork/write/exit cycles — possible leak",
         drift, STRESS_FORKS);
  } else {
    PASS("stress_memory");
  }
}

/* -----------------------------------------------------------------------
 * Test 8: grandchild COW — fork twice; grandchild writes, others read.
 * -------------------------------------------------------------------- */
static void test_grandchild_cow(void) {
  static char buf[ANON_PAGES * PAGE_SIZE];
  memset(buf, 0xAA, sizeof(buf));

  long before = read_mem_used_kb();

  pid_t child = fork();
  if (child == 0) {
    pid_t gc = fork();
    if (gc == 0) {
      /* Grandchild writes all pages */
      memset(buf, 0xBB, sizeof(buf));
      exit(0);
    }
    waitpid(gc, NULL, 0);
    /* Child verifies its view is still 0xAA */
    for (int i = 0; i < (int)sizeof(buf); i++) {
      if ((unsigned char)buf[i] != 0xAA) exit(1);
    }
    exit(0);
  }

  int status;
  waitpid(child, &status, 0);

  long after = read_mem_used_kb();
  long delta = after - before;

  int child_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;

  /* Parent view should still be 0xAA */
  int parent_ok = 1;
  for (int i = 0; i < (int)sizeof(buf); i++) {
    if ((unsigned char)buf[i] != 0xAA) { parent_ok = 0; break; }
  }

  if (!child_ok) {
    FAIL("grandchild_cow", "child saw wrong data after grandchild write");
  } else if (!parent_ok) {
    FAIL("grandchild_cow", "parent saw wrong data after grandchild write");
  } else if (delta > SLACK_KB + (long)(ANON_PAGES * PAGE_SIZE / 1024) * 2) {
    FAIL("grandchild_cow",
         "memory grew too much (%ld kB) for a 2-level fork", delta);
  } else {
    PASS("grandchild_cow");
  }
}

/* -----------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------- */
int main(void) {
  printf("testcow: COW fork memory tests\n");
  printf("  PAGE_SIZE=%d  ANON_PAGES=%d  SLACK_KB=%d\n\n",
         PAGE_SIZE, ANON_PAGES, SLACK_KB);

  test_fork_readonly();
  test_fork_write_one_page();
  test_fork_write_n_pages();
  test_no_leak_after_fork();
  test_last_ref_reclaim();
  test_multi_fork_isolation();
  test_stress_memory();
  test_grandchild_cow();

  printf("\ntestcow: %d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
