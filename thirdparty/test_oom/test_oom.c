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

/* QEMU virt machine has 128MB RAM. Map well beyond that to guarantee OOM. */
#define OOM_MAP_BYTES (256UL * 1024 * 1024)  /* 256 MB */

/* Expected exit code when kernel OOM-kills a process (128 + SIGKILL). */
#define OOM_EXIT_CODE (128 + SIGKILL)

/* Touch every page in [p, p+len) to force physical allocation. */
static void touch_pages(volatile char *p, size_t len) {
  for (size_t i = 0; i < len; i += 4096)
    p[i] = (char)i;
}

/*
 * OOM via anonymous page fault: child mmap's more RAM than exists and touches
 * every page. Kernel must kill it (exit 128+SIGKILL), not panic.
 */
static void test_oom_anon_fault(void) {
  SAY("  [1] OOM via anonymous page fault...\n");

  pid_t pid = fork();
  if (pid < 0) { FAIL("oom_anon_fault", "fork failed"); return; }

  if (pid == 0) {
    volatile char *p = mmap(NULL, OOM_MAP_BYTES, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == (void *)-1) _exit(1);
    touch_pages(p, OOM_MAP_BYTES);
    _exit(0); /* unreachable */
  }

  int status = 0;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status) && WEXITSTATUS(status) == OOM_EXIT_CODE)
    PASS("oom_anon_fault");
  else
    FAIL("oom_anon_fault", "exit=%d termsig=%d (want exit %d)",
         WEXITSTATUS(status), WTERMSIG(status), OOM_EXIT_CODE);
}

/*
 * OOM via COW fault: parent maps and touches a page, forks, then child
 * exhausts memory and triggers a COW write. Kernel must kill it, not panic.
 */
static void test_oom_cow_fault(void) {
  SAY("  [2] OOM via COW fault...\n");

  volatile char *shared = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (shared == (void *)-1) { FAIL("oom_cow_fault", "initial mmap failed"); return; }
  shared[0] = 42;

  pid_t pid = fork();
  if (pid < 0) { munmap((void *)shared, 4096); FAIL("oom_cow_fault", "fork failed"); return; }

  if (pid == 0) {
    /* Exhaust memory first, then trigger COW on the shared page. */
    volatile char *hog = mmap(NULL, OOM_MAP_BYTES, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (hog != (void *)-1)
      touch_pages(hog, OOM_MAP_BYTES);
    shared[0] = 99; /* COW fault — should OOM-kill us */
    _exit(0);
  }

  munmap((void *)shared, 4096);

  int status = 0;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status) && WEXITSTATUS(status) == OOM_EXIT_CODE)
    PASS("oom_cow_fault");
  else
    FAIL("oom_cow_fault", "exit=%d termsig=%d (want exit %d)",
         WEXITSTATUS(status), WTERMSIG(status), OOM_EXIT_CODE);
}

/*
 * OOM write bounce buffer: child exhausts memory then calls write().
 * The syscall must return (ENOMEM or success), not cause a kernel panic.
 * Child may also be OOM-killed during the touch loop — both outcomes are fine.
 */
static void test_oom_write_enomem(void) {
  SAY("  [3] OOM write syscall returns without panic...\n");

  pid_t pid = fork();
  if (pid < 0) { FAIL("oom_write_enomem", "fork failed"); return; }

  if (pid == 0) {
    volatile char *hog = mmap(NULL, OOM_MAP_BYTES, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (hog != (void *)-1)
      touch_pages(hog, OOM_MAP_BYTES);
    /* If we reach here, try a write — kernel needs a bounce page. */
    char buf[] = "x";
    ssize_t n = write(1, buf, 1);
    (void)n;
    _exit(0);
  }

  int status = 0;
  waitpid(pid, &status, 0);

  /* Any clean exit (including OOM kill) means no panic. */
  int no_panic = WIFEXITED(status) &&
                 (WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == OOM_EXIT_CODE);
  if (no_panic)
    PASS("oom_write_enomem");
  else
    FAIL("oom_write_enomem", "exit=%d termsig=%d",
         WEXITSTATUS(status), WTERMSIG(status));
}

/*
 * Kernel survives OOM: after an OOM child is reaped, the parent can still
 * allocate and use memory normally — pages were freed when the child died.
 */
static void test_kernel_survives_oom(void) {
  SAY("  [4] kernel survives OOM — allocation works after...\n");

  pid_t pid = fork();
  if (pid == 0) {
    volatile char *p = mmap(NULL, OOM_MAP_BYTES, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p != (void *)-1)
      touch_pages(p, OOM_MAP_BYTES);
    _exit(0);
  }
  int status;
  waitpid(pid, &status, 0);

  /* Allocate and use a single page — must succeed now that the child's
   * memory has been freed. */
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
