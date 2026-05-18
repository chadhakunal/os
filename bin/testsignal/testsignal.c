#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <sys/wait.h>

#define PASS(name)       printf("%s: PASS\n", name)
#define FAIL(name, ...)  printf("%s: FAIL - " __VA_ARGS__, name), printf("\n")

/* -----------------------------------------------------------------------
 * Test 1: SIGKILL terminates a child
 * -------------------------------------------------------------------- */
static void test_sigkill(void) {
  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }
  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGKILL);
  int status;
  pid_t r = waitpid(child, &status, 0);
  if (r == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL)
    PASS("test_sigkill");
  else
    FAIL("test_sigkill", "r=%d status=0x%x", r, status);
}

/* -----------------------------------------------------------------------
 * Test 2: SIGSTOP + SIGCONT
 * -------------------------------------------------------------------- */
static void test_sigstop_sigcont(void) {
  pid_t child = fork();
  if (child == 0) {
    for (volatile int i = 0; i < 5000000; i++);
    _exit(0);
  }
  for (int i = 0; i < 5; i++) sched_yield();
  kill(child, SIGSTOP);
  for (int i = 0; i < 20; i++) sched_yield();
  kill(child, SIGCONT);
  int status;
  pid_t r = waitpid(child, &status, 0);
  if (r == child && WIFEXITED(status) && WEXITSTATUS(status) == 0)
    PASS("test_sigstop_sigcont");
  else
    FAIL("test_sigstop_sigcont", "r=%d status=0x%x", r, status);
}

/* -----------------------------------------------------------------------
 * Test 3: SIGKILL wakes and kills a stopped process
 * -------------------------------------------------------------------- */
static void test_sigkill_stopped(void) {
  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }
  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGSTOP);
  for (int i = 0; i < 5; i++) sched_yield();
  kill(child, SIGKILL);
  int status;
  pid_t r = waitpid(child, &status, 0);
  if (r == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL)
    PASS("test_sigkill_stopped");
  else
    FAIL("test_sigkill_stopped", "r=%d status=0x%x", r, status);
}

/* -----------------------------------------------------------------------
 * Test 4: Stress — rapid stop/cont on multiple children
 * -------------------------------------------------------------------- */
static void test_stress_stop_cont(void) {
#define N_STRESS 4
  pid_t kids[N_STRESS];
  for (int i = 0; i < N_STRESS; i++) {
    kids[i] = fork();
    if (kids[i] == 0) {
      for (volatile long j = 0; j < 20000000L; j++);
      _exit(0);
    }
  }
  for (int round = 0; round < 8; round++) {
    for (int i = 0; i < N_STRESS; i++) kill(kids[i], SIGSTOP);
    for (int i = 0; i < N_STRESS; i++) kill(kids[i], SIGCONT);
  }
  int pass = 1;
  for (int i = 0; i < N_STRESS; i++) {
    int status;
    pid_t r = waitpid(kids[i], &status, 0);
    if (r != kids[i] || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      FAIL("test_stress_stop_cont", "child %d r=%d status=0x%x", kids[i], r, status);
      pass = 0;
    }
  }
  if (pass) PASS("test_stress_stop_cont");
#undef N_STRESS
}

/* -----------------------------------------------------------------------
 * Test 5: Custom signal handler runs and returns via sigreturn
 * -------------------------------------------------------------------- */
static volatile int handler5_count = 0;
static void handler5(int sig) {
  (void)sig;
  handler5_count++;
}

static void test_custom_handler(void) {
  handler5_count = 0;
  struct sigaction sa = { .sa_handler = handler5, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);

  raise(SIGUSR1);
  raise(SIGUSR1);

  /* Restore default */
  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);

  if (handler5_count == 2)
    PASS("test_custom_handler");
  else
    FAIL("test_custom_handler", "handler ran %d times (expected 2)", handler5_count);
}

/* -----------------------------------------------------------------------
 * Test 6: Signal masking — blocked signal not delivered until unblocked
 * -------------------------------------------------------------------- */
static volatile int handler6_count = 0;
static void handler6(int sig) { (void)sig; handler6_count++; }

static void test_signal_mask(void) {
  handler6_count = 0;
  struct sigaction sa = { .sa_handler = handler6, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);

  sigset_t block;
  sigemptyset(&block);
  sigaddset(&block, SIGUSR1);
  sigprocmask(SIG_BLOCK, &block, NULL);

  raise(SIGUSR1);
  raise(SIGUSR1);

  /* Handler must NOT have run yet */
  if (handler6_count != 0) {
    FAIL("test_signal_mask", "handler ran while signal was blocked");
    goto cleanup;
  }

  /* Unblock — both pending signals should be squashed into one delivery
   * (signals are not queued, just a pending bit) */
  sigprocmask(SIG_UNBLOCK, &block, NULL);

  if (handler6_count == 1)
    PASS("test_signal_mask");
  else
    FAIL("test_signal_mask", "expected 1 delivery after unblock, got %d", handler6_count);

cleanup:
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
}

/* -----------------------------------------------------------------------
 * Test 7: SIGCHLD — parent handler fires when child exits
 * -------------------------------------------------------------------- */
static volatile int handler7_count = 0;
static void handler7(int sig) { (void)sig; handler7_count++; }

static void test_sigchld(void) {
  handler7_count = 0;
  struct sigaction sa = { .sa_handler = handler7, .sa_flags = SA_RESTART };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGCHLD, &sa, NULL);

  pid_t child = fork();
  if (child == 0) _exit(0);

  int status;
  waitpid(child, &status, 0);

  if (handler7_count >= 1)
    PASS("test_sigchld");
  else
    FAIL("test_sigchld", "SIGCHLD handler never ran");

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGCHLD, &def, NULL);
}

/* -----------------------------------------------------------------------
 * Test 8: Multiple pending signals delivered in order
 * -------------------------------------------------------------------- */
static int order8[4];
static int order8_idx = 0;
static void handler8_usr1(int sig) { (void)sig; order8[order8_idx++] = SIGUSR1; }
static void handler8_usr2(int sig) { (void)sig; order8[order8_idx++] = SIGUSR2; }

static void test_multi_pending(void) {
  order8_idx = 0;
  struct sigaction sa1 = { .sa_handler = handler8_usr1, .sa_flags = 0 };
  struct sigaction sa2 = { .sa_handler = handler8_usr2, .sa_flags = 0 };
  sigemptyset(&sa1.sa_mask);
  sigemptyset(&sa2.sa_mask);
  sigaction(SIGUSR1, &sa1, NULL);
  sigaction(SIGUSR2, &sa2, NULL);

  /* Block both, queue both, then unblock */
  sigset_t block;
  sigemptyset(&block);
  sigaddset(&block, SIGUSR1);
  sigaddset(&block, SIGUSR2);
  sigprocmask(SIG_BLOCK, &block, NULL);

  raise(SIGUSR2);
  raise(SIGUSR1);

  sigprocmask(SIG_UNBLOCK, &block, NULL);

  /* Lower-numbered signal must be delivered first */
  if (order8_idx == 2 && order8[0] == SIGUSR1 && order8[1] == SIGUSR2)
    PASS("test_multi_pending");
  else
    FAIL("test_multi_pending", "delivery order wrong: [%d, %d] (idx=%d)",
         order8[0], order8[1], order8_idx);

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
  sigaction(SIGUSR2, &def, NULL);
}

/* -----------------------------------------------------------------------
 * Test 9: Signal interrupts a blocking waitpid (-EINTR)
 * -------------------------------------------------------------------- */
static void handler9(int sig) { (void)sig; }

static void test_eintr(void) {
  /* Install a handler for SIGUSR1 so it doesn't terminate us */
  struct sigaction sa = { .sa_handler = handler9, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);

  pid_t child = fork();
  if (child == 0) {
    /* Child: wait a bit, then send SIGUSR1 to parent, then spin */
    pid_t parent = getppid();
    for (int i = 0; i < 10; i++) sched_yield();
    kill(parent, SIGUSR1);
    while (1) sched_yield();
  }

  /* Parent blocks in waitpid; child sends SIGUSR1 which should interrupt it */
  int status;
  pid_t r = waitpid(child, &status, 0);

  int pass = (r == -1 && errno == EINTR);

  /* Clean up child */
  kill(child, SIGKILL);
  int s2;
  waitpid(child, &s2, 0);

  if (pass)
    PASS("test_eintr");
  else
    FAIL("test_eintr", "expected r=-1 errno=EINTR, got r=%d errno=%d", r, errno);

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
}

/* -----------------------------------------------------------------------
 * Test 10: SA_RESETHAND — handler resets to SIG_DFL after one delivery
 * -------------------------------------------------------------------- */
static volatile int handler10_count = 0;
static void handler10(int sig) { (void)sig; handler10_count++; }

static void test_sa_resethand(void) {
  handler10_count = 0;
  struct sigaction sa = { .sa_handler = handler10, .sa_flags = SA_RESETHAND };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);

  raise(SIGUSR1);
  /* Handler ran once; action should now be SIG_DFL.
   * A second SIGUSR1 with SIG_DFL would terminate the process, so
   * verify via sigaction that the handler was reset. */
  struct sigaction cur;
  sigemptyset(&cur.sa_mask);
  sigaction(SIGUSR1, NULL, &cur);

  if (handler10_count == 1 && cur.sa_handler == SIG_DFL)
    PASS("test_sa_resethand");
  else
    FAIL("test_sa_resethand", "count=%d handler=%p (expected SIG_DFL=%p)",
         handler10_count, cur.sa_handler, SIG_DFL);

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
}

/* -----------------------------------------------------------------------
 * Test 11: Nested signals — SIGUSR2 fires inside SIGUSR1 handler
 * -------------------------------------------------------------------- */
static volatile int nest_depth = 0;
static volatile int nest_max   = 0;
static volatile int nest_usr2  = 0;

static void handler11_usr2(int sig) {
  (void)sig;
  nest_usr2++;
}

static void handler11_usr1(int sig) {
  (void)sig;
  nest_depth++;
  if (nest_depth > nest_max) nest_max = nest_depth;
  raise(SIGUSR2);
  nest_depth--;
}

static void test_nested_signals(void) {
  nest_depth = nest_max = nest_usr2 = 0;

  struct sigaction sa1 = { .sa_handler = handler11_usr1, .sa_flags = SA_NODEFER };
  struct sigaction sa2 = { .sa_handler = handler11_usr2, .sa_flags = 0 };
  sigemptyset(&sa1.sa_mask);
  sigemptyset(&sa2.sa_mask);
  sigaction(SIGUSR1, &sa1, NULL);
  sigaction(SIGUSR2, &sa2, NULL);

  raise(SIGUSR1);

  if (nest_usr2 == 1)
    PASS("test_nested_signals");
  else
    FAIL("test_nested_signals", "SIGUSR2 fired %d times (expected 1)", nest_usr2);

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
  sigaction(SIGUSR2, &def, NULL);
}

/* -----------------------------------------------------------------------
 * Test 12: WUNTRACED — waitpid reports stopped child without waiting for exit
 * -------------------------------------------------------------------- */
static void test_wuntraced(void) {
  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }

  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGSTOP);

  int status;
  pid_t r = waitpid(child, &status, WUNTRACED);

  int pass = (r == child && WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP);

  /* Resume and clean up */
  kill(child, SIGCONT);
  kill(child, SIGKILL);
  int s2;
  waitpid(child, &s2, 0);

  if (pass)
    PASS("test_wuntraced");
  else
    FAIL("test_wuntraced", "r=%d status=0x%x WIFSTOPPED=%d WSTOPSIG=%d",
         r, status, WIFSTOPPED(status), WSTOPSIG(status));
}

/* -----------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------- */
int main(void) {
  printf("=== signal tests ===\n");
  test_sigkill();
  test_sigstop_sigcont();
  test_sigkill_stopped();
  test_stress_stop_cont();
  test_custom_handler();
  test_signal_mask();
  test_sigchld();
  test_multi_pending();
  test_eintr();
  test_sa_resethand();
  test_nested_signals();
  test_wuntraced();
  printf("=== done ===\n");
  return 0;
}
