#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <sys/wait.h>

#define N_RUNS 100

/* -----------------------------------------------------------------------
 * Test 1: SIGKILL terminates a child
 * -------------------------------------------------------------------- */
static int test_sigkill(void) {
  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }
  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGKILL);
  int status;
  pid_t r = waitpid(child, &status, 0);
  return r == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
}

/* -----------------------------------------------------------------------
 * Test 2: SIGSTOP + SIGCONT
 * -------------------------------------------------------------------- */
static int test_sigstop_sigcont(void) {
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
  return r == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* -----------------------------------------------------------------------
 * Test 3: SIGKILL wakes and kills a stopped process
 * -------------------------------------------------------------------- */
static int test_sigkill_stopped(void) {
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
  return r == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
}

/* -----------------------------------------------------------------------
 * Test 4: Stress — rapid stop/cont on multiple children
 * -------------------------------------------------------------------- */
static int test_stress_stop_cont(void) {
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
    if (r != kids[i] || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
      pass = 0;
  }
  return pass;
#undef N_STRESS
}

/* -----------------------------------------------------------------------
 * Test 5: Custom signal handler runs and returns via sigreturn
 * -------------------------------------------------------------------- */
static volatile int handler5_count;
static void handler5(int sig) { (void)sig; handler5_count++; }

static int test_custom_handler(void) {
  handler5_count = 0;
  struct sigaction sa = { .sa_handler = handler5, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);
  raise(SIGUSR1);
  raise(SIGUSR1);
  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
  return handler5_count == 2;
}

/* -----------------------------------------------------------------------
 * Test 6: Signal masking — blocked signal not delivered until unblocked
 * -------------------------------------------------------------------- */
static volatile int handler6_count;
static void handler6(int sig) { (void)sig; handler6_count++; }

static int test_signal_mask(void) {
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
  int still_blocked = (handler6_count == 0);
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  int after = handler6_count;

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
  return still_blocked && after == 1;
}

/* -----------------------------------------------------------------------
 * Test 7: SIGCHLD fires when child exits
 * -------------------------------------------------------------------- */
static volatile int handler7_count;
static void handler7(int sig) { (void)sig; handler7_count++; }

static int test_sigchld(void) {
  handler7_count = 0;
  struct sigaction sa = { .sa_handler = handler7, .sa_flags = SA_RESTART };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGCHLD, &sa, NULL);
  pid_t child = fork();
  if (child == 0) _exit(0);
  int status;
  waitpid(child, &status, 0);
  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGCHLD, &def, NULL);
  return handler7_count >= 1;
}

/* -----------------------------------------------------------------------
 * Test 8: Multiple pending signals delivered lowest-numbered first
 * -------------------------------------------------------------------- */
static int order8[4];
static int order8_idx;
static void handler8_usr1(int sig) { (void)sig; order8[order8_idx++] = SIGUSR1; }
static void handler8_usr2(int sig) { (void)sig; order8[order8_idx++] = SIGUSR2; }

static int test_multi_pending(void) {
  order8_idx = 0;
  struct sigaction sa1 = { .sa_handler = handler8_usr1, .sa_flags = 0 };
  struct sigaction sa2 = { .sa_handler = handler8_usr2, .sa_flags = 0 };
  sigemptyset(&sa1.sa_mask);
  sigemptyset(&sa2.sa_mask);
  sigaction(SIGUSR1, &sa1, NULL);
  sigaction(SIGUSR2, &sa2, NULL);

  sigset_t block;
  sigemptyset(&block);
  sigaddset(&block, SIGUSR1);
  sigaddset(&block, SIGUSR2);
  sigprocmask(SIG_BLOCK, &block, NULL);
  raise(SIGUSR2);
  raise(SIGUSR1);
  sigprocmask(SIG_UNBLOCK, &block, NULL);

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
  sigaction(SIGUSR2, &def, NULL);
  return order8_idx == 2 && order8[0] == SIGUSR1 && order8[1] == SIGUSR2;
}

/* -----------------------------------------------------------------------
 * Test 9: Signal interrupts a blocking waitpid (-EINTR)
 * -------------------------------------------------------------------- */
static void handler9(int sig) { (void)sig; }

static int test_eintr(void) {
  struct sigaction sa = { .sa_handler = handler9, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);

  pid_t child = fork();
  if (child == 0) {
    pid_t parent = getppid();
    for (int i = 0; i < 10; i++) sched_yield();
    kill(parent, SIGUSR1);
    while (1) sched_yield();
  }

  int status;
  pid_t r = waitpid(child, &status, 0);
  int pass = (r == -1 && errno == EINTR);

  kill(child, SIGKILL);
  int s2;
  waitpid(child, &s2, 0);

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
  return pass;
}

/* -----------------------------------------------------------------------
 * Test 10: SA_RESETHAND resets to SIG_DFL after one delivery
 * -------------------------------------------------------------------- */
static volatile int handler10_count;
static void handler10(int sig) { (void)sig; handler10_count++; }

static int test_sa_resethand(void) {
  handler10_count = 0;
  struct sigaction sa = { .sa_handler = handler10, .sa_flags = SA_RESETHAND };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);
  raise(SIGUSR1);
  struct sigaction cur;
  sigemptyset(&cur.sa_mask);
  sigaction(SIGUSR1, NULL, &cur);
  /* Ensure SIG_DFL is restored before returning */
  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
  return handler10_count == 1 && cur.sa_handler == SIG_DFL;
}

/* -----------------------------------------------------------------------
 * Test 11: Nested signals — SIGUSR2 fires inside SIGUSR1 handler
 * -------------------------------------------------------------------- */
static volatile int nest_usr2;
static void handler11_usr2(int sig) { (void)sig; nest_usr2++; }
static void handler11_usr1(int sig) { (void)sig; raise(SIGUSR2); }

static int test_nested_signals(void) {
  nest_usr2 = 0;
  struct sigaction sa1 = { .sa_handler = handler11_usr1, .sa_flags = SA_NODEFER };
  struct sigaction sa2 = { .sa_handler = handler11_usr2, .sa_flags = 0 };
  sigemptyset(&sa1.sa_mask);
  sigemptyset(&sa2.sa_mask);
  sigaction(SIGUSR1, &sa1, NULL);
  sigaction(SIGUSR2, &sa2, NULL);
  raise(SIGUSR1);
  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
  sigaction(SIGUSR2, &def, NULL);
  return nest_usr2 == 1;
}

/* -----------------------------------------------------------------------
 * Test 12: WUNTRACED reports stopped child without waiting for exit
 * -------------------------------------------------------------------- */
static int test_wuntraced(void) {
  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }
  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGSTOP);
  int status;
  pid_t r = waitpid(child, &status, WUNTRACED);
  int pass = (r == child && WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP);
  kill(child, SIGCONT);
  kill(child, SIGKILL);
  int s2;
  waitpid(child, &s2, 0);
  return pass;
}

/* -----------------------------------------------------------------------
 * Runner
 * -------------------------------------------------------------------- */
typedef int (*test_fn)(void);

static struct { const char *name; test_fn fn; } tests[] = {
  { "test_sigkill",         test_sigkill         },
  { "test_sigstop_sigcont", test_sigstop_sigcont  },
  { "test_sigkill_stopped", test_sigkill_stopped  },
  { "test_stress_stop_cont",test_stress_stop_cont },
  { "test_custom_handler",  test_custom_handler   },
  { "test_signal_mask",     test_signal_mask      },
  { "test_sigchld",         test_sigchld          },
  { "test_multi_pending",   test_multi_pending    },
  { "test_eintr",           test_eintr            },
  { "test_sa_resethand",    test_sa_resethand     },
  { "test_nested_signals",  test_nested_signals   },
  { "test_wuntraced",       test_wuntraced        },
};

int main(void) {
  printf("=== signal tests (%d runs each) ===\n", N_RUNS);
  int n = sizeof(tests) / sizeof(tests[0]);
  int any_failed = 0;

  for (int t = 0; t < n; t++) {
    int failures = 0;
    for (int i = 0; i < N_RUNS; i++) {
      if (!tests[t].fn())
        failures++;
    }
    if (failures == 0)
      printf("%s: PASS (%d/%d)\n", tests[t].name, N_RUNS, N_RUNS);
    else {
      printf("%s: FAIL (%d/%d failed)\n", tests[t].name, failures, N_RUNS);
      any_failed = 1;
    }
  }

  printf("=== %s ===\n", any_failed ? "FAILED" : "ALL PASS");
  return any_failed;
}
