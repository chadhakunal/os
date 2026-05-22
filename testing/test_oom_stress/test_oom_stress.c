/*
 * test_oom_stress: stress the OOM killer and RLIMIT_AS/ENOMEM paths.
 *
 * Each test runs a scenario repeatedly or with many concurrent children to
 * verify the kernel stays stable (no panics, no hangs, correct accounting)
 * under sustained pressure.
 *
 * QEMU virt machine has 128 MB RAM.
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

#define PAGE_SIZE        4096
#define MAP_ANON_PRIVATE (MAP_PRIVATE | MAP_ANONYMOUS)

/* Guaranteed to exceed 128 MB physical RAM. */
#define PHYS_EXHAUST_BYTES (256UL * 1024 * 1024)

/* A modest allocation that should always succeed after OOM children are reaped. */
#define SMALL_ALLOC_BYTES (64UL * 1024)

static int passed;
static int failed;

static void pass(const char *name) {
  printf("  PASS: %s\n", name); fflush(stdout); passed++;
}
static void fail(const char *name, const char *msg) {
  printf("  FAIL: %s -- %s\n", name, msg); fflush(stdout); failed++;
}

static void touch_pages(volatile char *p, size_t len) {
  for (size_t i = 0; i < len; i += PAGE_SIZE)
    p[i] = (char)i;
}

/* Returns 1 if the child was OOM-killed (SIGKILL), 0 otherwise. */
static int child_was_oom_killed(int status) {
  return WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
}

/* ------------------------------------------------------------------ */
/* [1] Repeated OOM kills: N sequential children each exhaust RAM.     */
/*     Kernel must survive every round without panicking.              */
/* ------------------------------------------------------------------ */
#define REPEATED_OOM_ROUNDS 5

static void test_repeated_oom_kills(void) {
  printf("  [1] repeated OOM kills (%d rounds)...\n", REPEATED_OOM_ROUNDS);
  const char *name = "repeated_oom_kills";

  int all_killed = 1;
  for (int i = 0; i < REPEATED_OOM_ROUNDS; i++) {
    pid_t pid = fork();
    if (pid < 0) { fail(name, "fork failed"); return; }

    if (pid == 0) {
      volatile char *p = mmap(NULL, PHYS_EXHAUST_BYTES, PROT_READ | PROT_WRITE,
                              MAP_ANON_PRIVATE, -1, 0);
      if (p == (void *)-1) _exit(42);
      touch_pages(p, PHYS_EXHAUST_BYTES);
      _exit(0);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (!child_was_oom_killed(status))
      all_killed = 0;
  }

  if (all_killed)
    pass(name);
  else
    fail(name, "at least one child was not OOM-killed");
}

/* ------------------------------------------------------------------ */
/* [2] Memory reclaimed after OOM: after each killed child, the parent */
/*     verifies it can still allocate and use a small region.          */
/* ------------------------------------------------------------------ */
static void test_memory_reclaimed_after_each_oom(void) {
  printf("  [2] memory reclaimed after each OOM kill (%d rounds)...\n", REPEATED_OOM_ROUNDS);
  const char *name = "memory_reclaimed_after_each_oom";

  int ok = 1;
  for (int i = 0; i < REPEATED_OOM_ROUNDS && ok; i++) {
    pid_t pid = fork();
    if (pid < 0) { fail(name, "fork failed"); return; }

    if (pid == 0) {
      volatile char *p = mmap(NULL, PHYS_EXHAUST_BYTES, PROT_READ | PROT_WRITE,
                              MAP_ANON_PRIVATE, -1, 0);
      if (p == (void *)-1) _exit(42);
      touch_pages(p, PHYS_EXHAUST_BYTES);
      _exit(0);
    }

    waitpid(pid, NULL, 0);

    /* Parent must be able to allocate after child's pages are freed. */
    void *check = mmap(NULL, SMALL_ALLOC_BYTES, PROT_READ | PROT_WRITE,
                       MAP_ANON_PRIVATE, -1, 0);
    if (check == (void *)-1) { ok = 0; break; }
    volatile char *c = (volatile char *)check;
    for (size_t j = 0; j < SMALL_ALLOC_BYTES; j += PAGE_SIZE)
      c[j] = (char)(i + j);
    munmap(check, SMALL_ALLOC_BYTES);
  }

  if (ok)
    pass(name);
  else
    fail(name, "parent could not allocate after a child OOM kill");
}

/* ------------------------------------------------------------------ */
/* [3] Concurrent OOM children: spawn N children simultaneously, all   */
/*     racing to exhaust physical RAM. All must be killed or complete; */
/*     the kernel must not panic or deadlock.                          */
/* ------------------------------------------------------------------ */
#define CONCURRENT_OOM_CHILDREN 4

static void test_concurrent_oom_children(void) {
  printf("  [3] %d concurrent OOM children...\n", CONCURRENT_OOM_CHILDREN);
  const char *name = "concurrent_oom_children";

  pid_t pids[CONCURRENT_OOM_CHILDREN];
  for (int i = 0; i < CONCURRENT_OOM_CHILDREN; i++) {
    pids[i] = fork();
    if (pids[i] < 0) {
      /* Reap what we already spawned before failing. */
      for (int j = 0; j < i; j++) waitpid(pids[j], NULL, 0);
      fail(name, "fork failed");
      return;
    }
    if (pids[i] == 0) {
      volatile char *p = mmap(NULL, PHYS_EXHAUST_BYTES, PROT_READ | PROT_WRITE,
                              MAP_ANON_PRIVATE, -1, 0);
      if (p == (void *)-1) _exit(42);
      touch_pages(p, PHYS_EXHAUST_BYTES);
      _exit(0);
    }
  }

  /* Reap all children — any exit (OOM kill or clean) means no panic/hang. */
  int oom_killed = 0;
  for (int i = 0; i < CONCURRENT_OOM_CHILDREN; i++) {
    int status = 0;
    waitpid(pids[i], &status, 0);
    if (child_was_oom_killed(status))
      oom_killed++;
  }

  /* At least one child must have been OOM-killed (RAM is only 128 MB). */
  if (oom_killed > 0)
    pass(name);
  else
    fail(name, "no children were OOM-killed — kernel may not have invoked OOM killer");
}

/* ------------------------------------------------------------------ */
/* [4] Rapid RLIMIT_AS set/get cycling: stress the rlimit syscalls     */
/*     themselves. No child — just the parent hammering setrlimit and  */
/*     getrlimit in a loop, checking consistency.                      */
/* ------------------------------------------------------------------ */
#define RLIMIT_CYCLE_ITERS 100

static void test_rlimit_setget_cycling(void) {
  printf("  [4] rapid RLIMIT_AS set/get cycling (%d iters)...\n", RLIMIT_CYCLE_ITERS);
  const char *name = "rlimit_setget_cycling";

  struct rlimit original;
  if (getrlimit(RLIMIT_AS, &original) != 0) {
    fail(name, "initial getrlimit failed");
    return;
  }

  int ok = 1;
  for (int i = 0; i < RLIMIT_CYCLE_ITERS && ok; i++) {
    /* Alternate between a small and a large soft limit. */
    uint64_t target = (i % 2 == 0) ? (uint64_t)(16 * PAGE_SIZE) : RLIM_INFINITY;
    struct rlimit rl = original;
    rl.rlim_cur = target;

    if (setrlimit(RLIMIT_AS, &rl) != 0) { ok = 0; break; }

    struct rlimit readback;
    if (getrlimit(RLIMIT_AS, &readback) != 0) { ok = 0; break; }
    if (readback.rlim_cur != target) { ok = 0; break; }
  }

  setrlimit(RLIMIT_AS, &original);

  if (ok)
    pass(name);
  else
    fail(name, "setrlimit/getrlimit returned inconsistent value during cycling");
}

/* ------------------------------------------------------------------ */
/* [5] Alternating ENOMEM and success: lower AS limit, confirm ENOMEM, */
/*     raise it, confirm success — repeated N times in the same        */
/*     process to stress the mmap/rlimit interaction.                  */
/* ------------------------------------------------------------------ */
#define ALTERNATING_ITERS 20

static void test_alternating_enomem_and_success(void) {
  printf("  [5] alternating ENOMEM and successful mmap (%d iters)...\n", ALTERNATING_ITERS);
  const char *name = "alternating_enomem_and_success";

  struct rlimit original;
  if (getrlimit(RLIMIT_AS, &original) != 0) {
    fail(name, "getrlimit failed");
    return;
  }

  int ok = 1;
  for (int i = 0; i < ALTERNATING_ITERS && ok; i++) {
    /* Low limit: mmap must fail. */
    struct rlimit low = original;
    low.rlim_cur = PAGE_SIZE;
    if (setrlimit(RLIMIT_AS, &low) != 0) { ok = 0; break; }

    void *p = mmap(NULL, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON_PRIVATE, -1, 0);
    if (p != (void *)-1) { munmap(p, 2 * PAGE_SIZE); ok = 0; break; }
    if (errno != ENOMEM) { ok = 0; break; }

    /* Restore: mmap must succeed. */
    if (setrlimit(RLIMIT_AS, &original) != 0) { ok = 0; break; }

    p = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON_PRIVATE, -1, 0);
    if (p == (void *)-1) { ok = 0; break; }
    ((volatile char *)p)[0] = (char)i;
    munmap(p, PAGE_SIZE);
  }

  setrlimit(RLIMIT_AS, &original);

  if (ok)
    pass(name);
  else
    fail(name, "ENOMEM/success pattern broke during iteration");
}

/* ------------------------------------------------------------------ */
/* [6] OOM kill + parent mmap interleaved: parent maps and uses small  */
/*     regions while sequentially waiting on OOM children. Verifies   */
/*     page accounting stays correct across many kill/reap cycles.    */
/* ------------------------------------------------------------------ */
#define INTERLEAVED_ROUNDS 4

static void test_oom_interleaved_with_parent_alloc(void) {
  printf("  [6] OOM kill interleaved with parent allocations (%d rounds)...\n", INTERLEAVED_ROUNDS);
  const char *name = "oom_interleaved_with_parent_alloc";

  int ok = 1;
  for (int i = 0; i < INTERLEAVED_ROUNDS && ok; i++) {
    /* Parent allocates before fork. */
    void *pre = mmap(NULL, SMALL_ALLOC_BYTES, PROT_READ | PROT_WRITE,
                     MAP_ANON_PRIVATE, -1, 0);
    if (pre == (void *)-1) { ok = 0; break; }
    touch_pages((volatile char *)pre, SMALL_ALLOC_BYTES);

    pid_t pid = fork();
    if (pid < 0) { munmap(pre, SMALL_ALLOC_BYTES); ok = 0; break; }

    if (pid == 0) {
      volatile char *p = mmap(NULL, PHYS_EXHAUST_BYTES, PROT_READ | PROT_WRITE,
                              MAP_ANON_PRIVATE, -1, 0);
      if (p == (void *)-1) _exit(42);
      touch_pages(p, PHYS_EXHAUST_BYTES);
      _exit(0);
    }

    /* Parent allocates after fork (before reap). */
    void *mid = mmap(NULL, SMALL_ALLOC_BYTES, PROT_READ | PROT_WRITE,
                     MAP_ANON_PRIVATE, -1, 0);
    /* mid may fail if RAM is already tight — that's acceptable. */

    waitpid(pid, NULL, 0);
    if (mid != (void *)-1) munmap(mid, SMALL_ALLOC_BYTES);
    munmap(pre, SMALL_ALLOC_BYTES);

    /* After reap, parent must be able to allocate. */
    void *post = mmap(NULL, SMALL_ALLOC_BYTES, PROT_READ | PROT_WRITE,
                      MAP_ANON_PRIVATE, -1, 0);
    if (post == (void *)-1) { ok = 0; break; }
    touch_pages((volatile char *)post, SMALL_ALLOC_BYTES);
    munmap(post, SMALL_ALLOC_BYTES);
  }

  if (ok)
    pass(name);
  else
    fail(name, "parent lost ability to allocate during interleaved OOM rounds");
}

/* ------------------------------------------------------------------ */
/* [7] RLIMIT_AS inherited across fork: child sees parent's limit.    */
/* ------------------------------------------------------------------ */
static void test_rlimit_inherited_by_child(void) {
  printf("  [7] RLIMIT_AS limit is inherited by child...\n");
  const char *name = "rlimit_inherited_by_child";

  struct rlimit saved;
  if (getrlimit(RLIMIT_AS, &saved) != 0) { fail(name, "getrlimit failed"); return; }

  struct rlimit low = saved;
  low.rlim_cur = PAGE_SIZE;
  if (setrlimit(RLIMIT_AS, &low) != 0) { fail(name, "setrlimit failed"); return; }

  pid_t pid = fork();
  if (pid < 0) { setrlimit(RLIMIT_AS, &saved); fail(name, "fork failed"); return; }

  if (pid == 0) {
    /*
     * Child must inherit the low soft limit. Any mmap > 1 page must fail.
     * Exit 0 = limit enforced, exit 1 = mmap unexpectedly succeeded.
     */
    void *p = mmap(NULL, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON_PRIVATE, -1, 0);
    if (p == (void *)-1 && errno == ENOMEM)
      _exit(0);
    if (p != (void *)-1) munmap(p, 2 * PAGE_SIZE);
    _exit(1);
  }

  setrlimit(RLIMIT_AS, &saved);

  int status = 0;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    pass(name);
  else
    fail(name, "child did not inherit RLIMIT_AS soft limit");
}

/* ------------------------------------------------------------------ */
/* [8] RLIMIT_AS change in child does not affect parent.              */
/* ------------------------------------------------------------------ */
static void test_rlimit_child_change_isolated(void) {
  printf("  [8] child RLIMIT_AS change does not affect parent...\n");
  const char *name = "rlimit_child_change_isolated";

  struct rlimit original;
  if (getrlimit(RLIMIT_AS, &original) != 0) { fail(name, "getrlimit failed"); return; }

  pid_t pid = fork();
  if (pid < 0) { fail(name, "fork failed"); return; }

  if (pid == 0) {
    struct rlimit low = original;
    low.rlim_cur = PAGE_SIZE;
    setrlimit(RLIMIT_AS, &low);
    _exit(0);
  }

  waitpid(pid, NULL, 0);

  /* Parent's limit must be unchanged. */
  struct rlimit after;
  if (getrlimit(RLIMIT_AS, &after) != 0) { fail(name, "getrlimit after fork failed"); return; }

  if (after.rlim_cur == original.rlim_cur)
    pass(name);
  else
    fail(name, "parent RLIMIT_AS soft limit changed after child modified it");
}

/* ------------------------------------------------------------------ */

int main(void) {
  printf("test_oom_stress: start\n\n");

  test_repeated_oom_kills();
  test_memory_reclaimed_after_each_oom();
  test_concurrent_oom_children();
  test_rlimit_setget_cycling();
  test_alternating_enomem_and_success();
  test_oom_interleaved_with_parent_alloc();
  test_rlimit_inherited_by_child();
  test_rlimit_child_change_isolated();

  printf("\ntest_oom_stress: %d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
