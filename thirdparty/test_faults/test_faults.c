#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
  if (ok) { printf("  [PASS] %s\n", name); passed++; }
  else     { printf("  [FAIL] %s\n", name); failed++; }
}

/* Returns 1 if the child was killed by the given signal. */
static int killed_by(int status, int sig) {
  return WIFSIGNALED(status) && WTERMSIG(status) == sig;
}

/* Fork a child that runs fn(), wait for it, return its wait status. */
static int run_child(void (*fn)(void)) {
  pid_t pid = fork();
  if (pid == 0) {
    fn();
    _exit(0); /* should not reach here if fn triggers a fault */
  }
  int status;
  waitpid(pid, &status, 0);
  return status;
}

/* -----------------------------------------------------------------------
 * SIGSEGV: null pointer dereference
 * -------------------------------------------------------------------- */
static void do_null_deref(void) {
  volatile int *p = (int *)0;
  (void)*p;
}

static void test_sigsegv_null(void) {
  printf("Test: SIGSEGV null deref\n");
  int st = run_child(do_null_deref);
  result("null deref: process killed", WIFSIGNALED(st));
  result("null deref: killed by SIGSEGV", killed_by(st, SIGSEGV));
}

/* -----------------------------------------------------------------------
 * SIGSEGV: write to read-only mapping (text segment)
 * -------------------------------------------------------------------- */
static void do_write_text(void) {
  /* The text segment is mapped read+exec but not write.
   * Try to write to the function pointer itself. */
  volatile char *p = (char *)(void *)do_write_text;
  *p = 0;
}

static void test_sigsegv_write_text(void) {
  printf("Test: SIGSEGV write to text segment\n");
  int st = run_child(do_write_text);
  result("write text: process killed", WIFSIGNALED(st));
  result("write text: killed by SIGSEGV", killed_by(st, SIGSEGV));
}

/* -----------------------------------------------------------------------
 * SIGSEGV: access unmapped high address
 * -------------------------------------------------------------------- */
static void do_high_addr(void) {
  volatile int *p = (int *)0xdeadbeef0000ULL;
  (void)*p;
}

static void test_sigsegv_unmapped(void) {
  printf("Test: SIGSEGV unmapped address\n");
  int st = run_child(do_high_addr);
  result("unmapped addr: process killed", WIFSIGNALED(st));
  result("unmapped addr: killed by SIGSEGV", killed_by(st, SIGSEGV));
}

/* -----------------------------------------------------------------------
 * SIGILL: illegal instruction via inline asm
 * -------------------------------------------------------------------- */
static void do_illegal_insn(void) {
  /* RISC-V: all-zero word is an illegal instruction */
  asm volatile(".word 0x00000000");
}

static void test_sigill(void) {
  printf("Test: SIGILL illegal instruction\n");
  int st = run_child(do_illegal_insn);
  result("illegal insn: process killed", WIFSIGNALED(st));
  result("illegal insn: killed by SIGILL", killed_by(st, SIGILL));
}

/* -----------------------------------------------------------------------
 * SIGBUS: misaligned load
 * -------------------------------------------------------------------- */
static void do_misaligned_load(void) {
  static char buf[8] = {0};
  /* Force a misaligned 4-byte load from an odd address */
  volatile int *p = (int *)(buf + 1);
  (void)*p;
}

static void test_sigbus(void) {
  printf("Test: SIGBUS misaligned access\n");
  int st = run_child(do_misaligned_load);
  /* RISC-V hardware raises misaligned exception only for accesses the hart
   * cannot handle — some implementations handle it in hardware.
   * Either SIGBUS or clean exit (if handled in hw) is acceptable here,
   * but it must NOT kill the kernel. */
  result("misaligned: process did not hang", WIFEXITED(st) || WIFSIGNALED(st));
  if (WIFSIGNALED(st))
    result("misaligned: killed by SIGBUS if signaled", WTERMSIG(st) == SIGBUS);
  else {
    /* hw handled it — just record a pass */
    result("misaligned: hw handled (no signal)", 1);
  }
}

/* -----------------------------------------------------------------------
 * Custom SIGSEGV handler: process catches and recovers
 * -------------------------------------------------------------------- */
static volatile int segv_caught = 0;

static void segv_handler(int sig) {
  (void)sig;
  segv_caught = 1;
  /* We can't safely return to the faulting instruction — just exit cleanly */
  _exit(42);
}

static void do_catch_sigsegv(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = segv_handler;
  sigaction(SIGSEGV, &sa, NULL);

  volatile int *p = (int *)0;
  (void)*p;
  /* Should not reach here */
  _exit(1);
}

static void test_sigsegv_caught(void) {
  printf("Test: custom SIGSEGV handler\n");
  pid_t pid = fork();
  if (pid == 0) {
    do_catch_sigsegv();
    _exit(1);
  }
  int st;
  waitpid(pid, &st, 0);
  result("caught SIGSEGV: handler ran (exit 42)", WIFEXITED(st) && WEXITSTATUS(st) == 42);
}

/* -----------------------------------------------------------------------
 * Custom SIGILL handler
 * -------------------------------------------------------------------- */
static void ill_handler(int sig) {
  (void)sig;
  _exit(43);
}

static void do_catch_sigill(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = ill_handler;
  sigaction(SIGILL, &sa, NULL);

  asm volatile(".word 0x00000000");
  _exit(1);
}

static void test_sigill_caught(void) {
  printf("Test: custom SIGILL handler\n");
  pid_t pid = fork();
  if (pid == 0) {
    do_catch_sigill();
    _exit(1);
  }
  int st;
  waitpid(pid, &st, 0);
  result("caught SIGILL: handler ran (exit 43)", WIFEXITED(st) && WEXITSTATUS(st) == 43);
}

/* -----------------------------------------------------------------------
 * SA_RESETHAND: handler fires once, then resets to default
 * -------------------------------------------------------------------- */
static int resethand_count = 0;

static void resethand_handler(int sig) {
  (void)sig;
  resethand_count++;
  /* Return — signal is now reset to SIG_DFL.
   * The second fault will kill us via default SIGILL action. */
}

static void do_resethand(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = resethand_handler;
  sa.sa_flags = SA_RESETHAND;
  sigaction(SIGILL, &sa, NULL);

  /* First illegal instruction — handler fires, then resets */
  asm volatile(".word 0x00000000");
  /* Second illegal instruction — default action kills us */
  asm volatile(".word 0x00000000");
  _exit(1); /* should not reach */
}

static void test_sa_resethand(void) {
  printf("Test: SA_RESETHAND\n");
  int st = run_child(do_resethand);
  /* Child should be killed by SIGILL on the second fault */
  result("SA_RESETHAND: killed by SIGILL on second fault", killed_by(st, SIGILL));
}

/* -----------------------------------------------------------------------
 * Sibling isolation: one child faults, other keeps running
 * -------------------------------------------------------------------- */
static void do_fault_and_exit(void) {
  volatile int *p = (int *)0;
  (void)*p;
}

static void do_spin_then_exit(void) {
  /* Burn a few loops then exit cleanly */
  volatile int x = 0;
  for (int i = 0; i < 100000; i++) x++;
  (void)x;
  _exit(0);
}

static void test_sibling_isolation(void) {
  printf("Test: sibling isolation\n");

  pid_t faulter  = fork();
  if (faulter == 0) { do_fault_and_exit(); _exit(0); }

  pid_t survivor = fork();
  if (survivor == 0) { do_spin_then_exit(); _exit(0); }

  int st_f, st_s;
  waitpid(faulter,  &st_f, 0);
  waitpid(survivor, &st_s, 0);

  result("sibling isolation: faulter killed by SIGSEGV", killed_by(st_f, SIGSEGV));
  result("sibling isolation: survivor exits cleanly",    WIFEXITED(st_s) && WEXITSTATUS(st_s) == 0);
}

/* -----------------------------------------------------------------------
 * SIGSEGV on stack overflow: write below the stack guard
 * -------------------------------------------------------------------- */
static void do_stack_overflow(void) {
  /* Stack is at 0x7FFF0000-0x80000000 (64KB).
   * Write directly below the bottom of the stack mapping. */
  volatile char *p = (volatile char *)0x7FFE0000ULL;
  *p = 1;
}

static void test_stack_overflow(void) {
  printf("Test: stack overflow\n");
  int st = run_child(do_stack_overflow);
  result("stack overflow: process killed", WIFSIGNALED(st));
  result("stack overflow: killed by SIGSEGV", killed_by(st, SIGSEGV));
}

/* -----------------------------------------------------------------------
 * Normal process unaffected after sibling fault
 * -------------------------------------------------------------------- */
static void test_kernel_survives(void) {
  printf("Test: kernel survives multiple faults\n");

  /* Run three faulting children sequentially */
  for (int i = 0; i < 3; i++) {
    int st = run_child(do_null_deref);
    if (!killed_by(st, SIGSEGV)) {
      result("kernel survives: child killed correctly", 0);
      return;
    }
  }
  result("kernel survives: all three faulters killed cleanly", 1);

  /* Verify normal process still works */
  pid_t pid = fork();
  if (pid == 0) _exit(7);
  int st;
  waitpid(pid, &st, 0);
  result("kernel survives: normal child exits fine", WIFEXITED(st) && WEXITSTATUS(st) == 7);
}

int main(void) {
  printf("=== fault signal tests ===\n");
  test_sigsegv_null();
  test_sigsegv_write_text();
  test_sigsegv_unmapped();
  test_sigill();
  test_sigbus();
  test_sigsegv_caught();
  test_sigill_caught();
  test_sa_resethand();
  test_sibling_isolation();
  test_stack_overflow();
  test_kernel_survives();
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
