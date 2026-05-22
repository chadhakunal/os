#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
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
 * Test 13: Scheduler stress — many processes forking/exiting while
 * signals fly between them.
 * -------------------------------------------------------------------- */
static int test_scheduler_stress(void) {
#define N_SCHED 8
  pid_t kids[N_SCHED];
  for (int i = 0; i < N_SCHED; i++) {
    kids[i] = fork();
    if (kids[i] == 0) {
      /* Each child yields a few times then exits. */
      for (int j = 0; j < 5; j++) sched_yield();
      _exit(0);
    }
  }
  /* While children run, rapidly send SIGUSR1 to each (ignored by default
   * — just stresses signal delivery path under scheduler churn). */
  for (int round = 0; round < 4; round++) {
    for (int i = 0; i < N_SCHED; i++)
      kill(kids[i], SIGUSR1);
    sched_yield();
  }
  int pass = 1;
  for (int i = 0; i < N_SCHED; i++) {
    int status;
    pid_t r = waitpid(kids[i], &status, 0);
    /* SIGUSR1 default action is terminate — child may exit via signal or
     * normally; either is acceptable here, we just want no hang/panic. */
    if (r != kids[i])
      pass = 0;
  }
  return pass;
#undef N_SCHED
}

/* -----------------------------------------------------------------------
 * Test 14: Scheduler stress with stop/cont — many children stopped and
 * continued while new forks are happening.
 * -------------------------------------------------------------------- */
static int test_scheduler_stress_stop(void) {
#define N_SC 6
  pid_t kids[N_SC];
  for (int i = 0; i < N_SC; i++) {
    kids[i] = fork();
    if (kids[i] == 0) {
      for (volatile long j = 0; j < 10000000L; j++);
      _exit(0);
    }
  }
  /* Stop half, cont them, fork more, repeat. */
  for (int round = 0; round < 4; round++) {
    for (int i = 0; i < N_SC / 2; i++) kill(kids[i], SIGSTOP);
    sched_yield();
    for (int i = 0; i < N_SC / 2; i++) kill(kids[i], SIGCONT);
    sched_yield();
  }
  int pass = 1;
  for (int i = 0; i < N_SC; i++) {
    int status;
    pid_t r = waitpid(kids[i], &status, 0);
    if (r != kids[i] || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
      pass = 0;
  }
  return pass;
#undef N_SC
}

/* -----------------------------------------------------------------------
 * Test 15: Signal handler that calls malloc — memory allocator must be
 * safe to call from a signal handler (single-threaded, so re-entrancy
 * is not an issue here, but the handler must not corrupt the heap).
 * -------------------------------------------------------------------- */
static volatile int handler15_ran;
static void handler15(int sig) {
  (void)sig;
  /* Allocate and free inside the handler. */
  char *p = malloc(256);
  if (p) {
    for (int i = 0; i < 256; i++) p[i] = (char)i;
    free(p);
    handler15_ran = 1;
  }
}

static int test_signal_malloc(void) {
  handler15_ran = 0;
  struct sigaction sa = { .sa_handler = handler15, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);
  /* Do a heap allocation before and after to check the heap isn't corrupted. */
  char *pre = malloc(128);
  if (!pre) return 0;
  for (int i = 0; i < 128; i++) pre[i] = (char)i;
  raise(SIGUSR1);
  /* Verify pre-signal allocation is intact. */
  int ok = handler15_ran;
  for (int i = 0; i < 128; i++) if (pre[i] != (char)i) { ok = 0; break; }
  free(pre);
  char *post = malloc(128);
  if (!post) return 0;
  free(post);
  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);
  return ok;
}

/* -----------------------------------------------------------------------
 * Test 16: Signal handler allocates, main allocates concurrently across
 * many raises — stress the heap across repeated handler invocations.
 * -------------------------------------------------------------------- */
static volatile int handler16_failures;
static void handler16(int sig) {
  (void)sig;
  void *p = malloc(64);
  if (!p) { handler16_failures++; return; }
  free(p);
}

static int test_signal_malloc_stress(void) {
  handler16_failures = 0;
  struct sigaction sa = { .sa_handler = handler16, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR2, &sa, NULL);
  /* Interleave malloc in main with raises. */
  void *ptrs[16];
  for (int i = 0; i < 16; i++) {
    ptrs[i] = malloc(32 * (i + 1));
    raise(SIGUSR2);
  }
  for (int i = 0; i < 16; i++) free(ptrs[i]);
  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR2, &def, NULL);
  return handler16_failures == 0;
}

/* -----------------------------------------------------------------------
 * Test 17: nanosleep interrupted by signal returns -1/EINTR.
 * -------------------------------------------------------------------- */
static void handler17(int sig) { (void)sig; }

static int test_nanosleep_eintr(void) {
  struct sigaction sa = { .sa_handler = handler17, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);

  pid_t child = fork();
  if (child == 0) {
    pid_t parent = getppid();
    /* Give parent time to enter nanosleep. */
    for (int i = 0; i < 5; i++) sched_yield();
    kill(parent, SIGUSR1);
    _exit(0);
  }

  struct timespec req = { .tv_sec = 10, .tv_nsec = 0 };
  struct timespec rem = { 0, 0 };
  int r = nanosleep(&req, &rem);

  /* Reap child regardless of outcome. */
  int s;
  waitpid(child, &s, 0);

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);

  /* nanosleep should return -1 with errno == EINTR */
  return r == -1 && errno == EINTR;
}

/* -----------------------------------------------------------------------
 * Test 18: nanosleep completes normally (not interrupted).
 * -------------------------------------------------------------------- */
static int test_nanosleep_completes(void) {
  struct timespec req = { .tv_sec = 0, .tv_nsec = 50000000 }; /* 50ms */
  int r = nanosleep(&req, NULL);
  return r == 0;
}

/* -----------------------------------------------------------------------
 * Test 19: SIGPIPE — writing to a pipe with no readers sends SIGPIPE
 * and write returns -1/EPIPE.
 * -------------------------------------------------------------------- */
static volatile int handler19_ran;
static void handler19(int sig) { (void)sig; handler19_ran = 1; }

static int test_sigpipe(void) {
  handler19_ran = 0;
  struct sigaction sa = { .sa_handler = handler19, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGPIPE, &sa, NULL);

  int fds[2];
  if (pipe(fds) < 0) return 0;

  /* Close the read end — pipe is now broken. */
  close(fds[0]);

  char buf[4] = "test";
  int r = write(fds[1], buf, 4);
  close(fds[1]);

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGPIPE, &def, NULL);

  /* write must return -1 and SIGPIPE handler must have run */
  return r == -1 && handler19_ran == 1;
}

/* -----------------------------------------------------------------------
 * Test 20: SIGPIPE ignored — write returns -1/EPIPE without terminating.
 * -------------------------------------------------------------------- */
static int test_sigpipe_ignored(void) {
  struct sigaction sa = { .sa_handler = SIG_IGN, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGPIPE, &sa, NULL);

  int fds[2];
  if (pipe(fds) < 0) return 0;
  close(fds[0]);

  char buf[4] = "test";
  int r = write(fds[1], buf, 4);
  close(fds[1]);

  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGPIPE, &def, NULL);

  return r == -1 && errno == EPIPE;
}

/* -----------------------------------------------------------------------
 * Test 21: Process groups — kill(-pgid) delivers signal to all members.
 * -------------------------------------------------------------------- */
static int test_kill_pgid(void) {
#define N_PG 4
  /* Put all children in a new process group. */
  pid_t kids[N_PG];
  pid_t new_pgid = 0;

  for (int i = 0; i < N_PG; i++) {
    kids[i] = fork();
    if (kids[i] == 0) {
      /* First child becomes the process group leader. */
      if (i == 0) setpgid(0, 0);
      else        setpgid(0, kids[0] ? kids[0] : getpid());
      while (1) sched_yield();
    }
    if (i == 0) new_pgid = kids[0];
  }

  /* Give children time to call setpgid. */
  for (int i = 0; i < 10; i++) sched_yield();

  /* Kill the whole group with SIGKILL. */
  kill(-new_pgid, SIGKILL);

  int pass = 1;
  for (int i = 0; i < N_PG; i++) {
    int status;
    pid_t r = waitpid(kids[i], &status, 0);
    if (r != kids[i] || !WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
      pass = 0;
  }
  return pass;
#undef N_PG
}

/* -----------------------------------------------------------------------
 * Test 22: setpgid isolation — child moves to its own group, parent's
 * kill(-pgid) does NOT reach it.
 * -------------------------------------------------------------------- */
static int test_pgid_isolation(void) {
  pid_t child = fork();
  if (child == 0) {
    /* Move to own process group — out of parent's group. */
    setpgid(0, 0);
    /* Sleep long enough that the parent's kill fires, then exit cleanly. */
    for (volatile long i = 0; i < 30000000L; i++);
    _exit(0);
  }

  /* Give child time to call setpgid. */
  for (int i = 0; i < 5; i++) sched_yield();

  /* Kill parent's own pgid — should NOT reach the child. */
  kill(-getpgid(0), SIGUSR1);   /* SIGUSR1: default=terminate, proves isolation */

  int status;
  pid_t r = waitpid(child, &status, 0);

  /* Child should have exited cleanly (not via SIGUSR1). */
  return r == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* -----------------------------------------------------------------------
 * Test 23: Signal inheritance across fork — child inherits blocked mask
 * and dispositions; pending signals are NOT inherited.
 * -------------------------------------------------------------------- */
static volatile int handler23_count;
static void handler23(int sig) { (void)sig; handler23_count++; }

static int test_signal_inheritance(void) {
  /* Set up: block SIGUSR2, install handler for SIGUSR1. */
  struct sigaction sa = { .sa_handler = handler23, .sa_flags = 0 };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);

  sigset_t block;
  sigemptyset(&block);
  sigaddset(&block, SIGUSR2);
  sigprocmask(SIG_BLOCK, &block, NULL);

  /* Queue a SIGUSR1 to parent (pending) before fork. */
  sigset_t also_block;
  sigemptyset(&also_block);
  sigaddset(&also_block, SIGUSR1);
  sigprocmask(SIG_BLOCK, &also_block, NULL);
  raise(SIGUSR1);  /* now pending in parent */

  int fds[2];
  pipe(fds);

  pid_t child = fork();
  if (child == 0) {
    close(fds[0]);
    /* Child: SIGUSR1 should NOT be pending (pending not inherited).
     * SIGUSR2 should still be blocked (mask inherited).
     * SIGUSR1 handler should be inherited. */
    sigset_t pending;
    sigpending(&pending);
    int sigusr1_pending = sigismember(&pending, SIGUSR1);

    sigset_t cur_blocked;
    sigprocmask(SIG_SETMASK, NULL, &cur_blocked);
    int sigusr2_blocked = sigismember(&cur_blocked, SIGUSR2);

    /* Check handler inherited: install nothing, check current action. */
    struct sigaction cur_act;
    sigaction(SIGUSR1, NULL, &cur_act);
    int handler_inherited = (cur_act.sa_handler == handler23);

    uint8_t result = (!sigusr1_pending && sigusr2_blocked && handler_inherited) ? 1 : 0;
    write(fds[1], &result, 1);
    close(fds[1]);
    _exit(0);
  }

  close(fds[1]);

  /* Unblock SIGUSR1 in parent — delivers the queued signal. */
  sigprocmask(SIG_UNBLOCK, &also_block, NULL);

  uint8_t child_result = 0;
  read(fds[0], &child_result, 1);
  close(fds[0]);

  int status;
  waitpid(child, &status, 0);

  /* Restore parent state. */
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  struct sigaction def = { .sa_handler = SIG_DFL, .sa_flags = 0 };
  sigemptyset(&def.sa_mask);
  sigaction(SIGUSR1, &def, NULL);

  return child_result == 1;
}

/* -----------------------------------------------------------------------
 * Runner
 * -------------------------------------------------------------------- */
typedef int (*test_fn)(void);

static struct { const char *name; test_fn fn; } tests[] = {
  { "test_sigkill",              test_sigkill              },
  { "test_sigstop_sigcont",      test_sigstop_sigcont      },
  { "test_sigkill_stopped",      test_sigkill_stopped      },
  { "test_stress_stop_cont",     test_stress_stop_cont     },
  { "test_custom_handler",       test_custom_handler       },
  { "test_signal_mask",          test_signal_mask          },
  { "test_sigchld",              test_sigchld              },
  { "test_multi_pending",        test_multi_pending        },
  { "test_eintr",                test_eintr                },
  { "test_sa_resethand",         test_sa_resethand         },
  { "test_nested_signals",       test_nested_signals       },
  { "test_wuntraced",            test_wuntraced            },
  { "test_scheduler_stress",     test_scheduler_stress     },
  { "test_scheduler_stress_stop",test_scheduler_stress_stop},
  { "test_signal_malloc",        test_signal_malloc        },
  { "test_signal_malloc_stress", test_signal_malloc_stress },
  { "test_nanosleep_eintr",      test_nanosleep_eintr      },
  { "test_nanosleep_completes",  test_nanosleep_completes  },
  { "test_sigpipe",              test_sigpipe              },
  { "test_sigpipe_ignored",      test_sigpipe_ignored      },
  { "test_kill_pgid",            test_kill_pgid            },
  { "test_pgid_isolation",       test_pgid_isolation       },
  { "test_signal_inheritance",   test_signal_inheritance   },
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
