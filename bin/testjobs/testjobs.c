#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>

#define N_RUNS 1000

/* -----------------------------------------------------------------------
 * Test 1: SIGTSTP stops child, WUNTRACED reports it.
 * -------------------------------------------------------------------- */
static int test_tstp_wuntraced(void) {
  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }
  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGTSTP);

  int status;
  pid_t r = 0;
  for (int i = 0; i < 50; i++) {
    sched_yield();
    r = waitpid(child, &status, WUNTRACED | WNOHANG);
    if (r == child) break;
  }
  if (r != child || !WIFSTOPPED(status) || WSTOPSIG(status) != SIGTSTP) {
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    return 0;
  }
  kill(child, SIGKILL);
  waitpid(child, NULL, 0);
  return 1;
}

/* -----------------------------------------------------------------------
 * Test 2: SIGTSTP stops child, SIGCONT resumes it, child exits cleanly.
 * -------------------------------------------------------------------- */
static int test_tstp_resume_exit(void) {
  pid_t child = fork();
  if (child == 0) {
    /* Loop forever via sched_yield so SIGTSTP is always deliverable. */
    while (1) sched_yield();
  }
  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGTSTP);

  /* Poll until child is stopped. */
  int status;
  pid_t r = 0;
  for (int i = 0; i < 50; i++) {
    sched_yield();
    r = waitpid(child, &status, WUNTRACED | WNOHANG);
    if (r == child) break;
  }
  if (r != child || !WIFSTOPPED(status)) {
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    return 0;
  }

  /* Resume and kill — verify it was alive and resumable. */
  kill(child, SIGCONT);
  for (int i = 0; i < 5; i++) sched_yield();
  kill(child, SIGKILL);
  r = waitpid(child, &status, 0);
  return r == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
}

/* -----------------------------------------------------------------------
 * Test 3: SIGTSTP to a whole process group stops all members.
 * -------------------------------------------------------------------- */
static int test_tstp_pgid(void) {
  pid_t child = fork();
  if (child == 0) {
    setpgid(0, 0);
    pid_t grandchild = fork();
    if (grandchild == 0) {
      while (1) sched_yield();
    }
    while (1) sched_yield();
  }

  for (int i = 0; i < 5; i++) sched_yield();
  kill(-child, SIGTSTP);

  int status;
  pid_t r = 0;
  for (int i = 0; i < 50; i++) {
    sched_yield();
    r = waitpid(child, &status, WUNTRACED | WNOHANG);
    if (r == child) break;
  }
  if (r != child || !WIFSTOPPED(status)) {
    kill(-child, SIGKILL);
    waitpid(child, NULL, 0);
    return 0;
  }

  kill(-child, SIGKILL);
  waitpid(child, &status, 0);
  return WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
}

/* -----------------------------------------------------------------------
 * Test 4: Multiple stop/cont cycles — child survives N rounds then exits.
 * -------------------------------------------------------------------- */
static int test_tstp_multi_cycle(void) {
  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }

  for (int round = 0; round < 5; round++) {
    for (int i = 0; i < 3; i++) sched_yield();
    kill(child, SIGTSTP);

    int status;
    pid_t r = 0;
    for (int i = 0; i < 50; i++) {
      sched_yield();
      r = waitpid(child, &status, WUNTRACED | WNOHANG);
      if (r == child) break;
    }
    if (r != child || !WIFSTOPPED(status)) {
      kill(child, SIGKILL);
      waitpid(child, NULL, 0);
      return 0;
    }
    kill(child, SIGCONT);
  }

  for (int i = 0; i < 5; i++) sched_yield();
  kill(child, SIGKILL);
  int status;
  pid_t r = waitpid(child, &status, 0);
  return r == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
}

/* -----------------------------------------------------------------------
 * Test 5: SIGSTOP (unblockable) stops child, WUNTRACED reports it.
 * -------------------------------------------------------------------- */
static int test_sigstop_wuntraced(void) {
  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }
  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGSTOP);

  int status;
  pid_t r = 0;
  for (int i = 0; i < 50; i++) {
    sched_yield();
    r = waitpid(child, &status, WUNTRACED | WNOHANG);
    if (r == child) break;
  }
  if (r != child || !WIFSTOPPED(status)) {
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    return 0;
  }
  kill(child, SIGKILL);
  waitpid(child, NULL, 0);
  return 1;
}

/* -----------------------------------------------------------------------
 * Test 6: Stress — rapid stop/cont on many children.
 * -------------------------------------------------------------------- */
static int test_tstp_stress(void) {
#define N_STRESS 8
  pid_t kids[N_STRESS];
  for (int i = 0; i < N_STRESS; i++) {
    kids[i] = fork();
    if (kids[i] == 0) {
      while (1) sched_yield();
    }
  }

  for (int round = 0; round < 3; round++) {
    for (int i = 0; i < 3; i++) sched_yield();
    for (int i = 0; i < N_STRESS; i++) kill(kids[i], SIGTSTP);
    for (int i = 0; i < 5; i++) sched_yield();
    for (int i = 0; i < N_STRESS; i++) kill(kids[i], SIGCONT);
  }

  int pass = 1;
  for (int i = 0; i < N_STRESS; i++) {
    kill(kids[i], SIGKILL);
    int status;
    pid_t r = waitpid(kids[i], &status, 0);
    if (r != kids[i] || !WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
      pass = 0;
  }
  return pass;
#undef N_STRESS
}

/* -----------------------------------------------------------------------
 * Test 7: SIGCONT on a running process is a no-op — child still exits 0.
 * -------------------------------------------------------------------- */
static int test_sigcont_noop(void) {
  pid_t child = fork();
  if (child == 0) {
    for (volatile int i = 0; i < 100000; i++);
    _exit(0);
  }
  for (int i = 0; i < 2; i++) sched_yield();
  kill(child, SIGCONT);  /* should do nothing, child is already running */
  int status;
  pid_t r = waitpid(child, &status, 0);
  return r == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* -----------------------------------------------------------------------
 * Test 8: stop_reported — WUNTRACED only reports a stop once.
 * -------------------------------------------------------------------- */
static int test_stop_reported_once(void) {
  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }
  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGTSTP);

  /* First waitpid should report the stop. */
  int status;
  pid_t r = 0;
  for (int i = 0; i < 50; i++) {
    sched_yield();
    r = waitpid(child, &status, WUNTRACED | WNOHANG);
    if (r == child) break;
  }
  if (r != child || !WIFSTOPPED(status)) {
    kill(child, SIGKILL); waitpid(child, NULL, 0); return 0;
  }

  /* Second waitpid should NOT report the same stop again. */
  r = waitpid(child, &status, WUNTRACED | WNOHANG);
  if (r != 0) {
    kill(child, SIGKILL); waitpid(child, NULL, 0); return 0;
  }

  kill(child, SIGKILL);
  waitpid(child, NULL, 0);
  return 1;
}

/* -----------------------------------------------------------------------
 * Test 9: SIGKILL wakes and kills a SIGTSTP-stopped process.
 * -------------------------------------------------------------------- */
static int test_sigkill_wakes_stopped(void) {
  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }
  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGTSTP);

  int status;
  pid_t r = 0;
  for (int i = 0; i < 50; i++) {
    sched_yield();
    r = waitpid(child, &status, WUNTRACED | WNOHANG);
    if (r == child) break;
  }
  if (r != child || !WIFSTOPPED(status)) {
    kill(child, SIGKILL); waitpid(child, NULL, 0); return 0;
  }

  kill(child, SIGKILL);
  r = waitpid(child, &status, 0);
  return r == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
}

/* -----------------------------------------------------------------------
 * Test 10: Child ignoring SIGTSTP cannot be stopped by it.
 * -------------------------------------------------------------------- */
static int test_tstp_ignored(void) {
  pid_t child = fork();
  if (child == 0) {
    signal(SIGTSTP, SIG_IGN);
    while (1) sched_yield();
  }
  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGTSTP);

  /* Give signal time to be delivered — child should stay running. */
  for (int i = 0; i < 20; i++) sched_yield();

  /* Should NOT be stopped. */
  int status;
  pid_t r = waitpid(child, &status, WUNTRACED | WNOHANG);
  if (r == child && WIFSTOPPED(status)) {
    kill(child, SIGKILL); waitpid(child, NULL, 0); return 0;
  }

  kill(child, SIGKILL);
  waitpid(child, NULL, 0);
  return 1;
}

/* -----------------------------------------------------------------------
 * Test 11: Stop a child that is blocked in a pipe read — SIGCONT resumes
 * it, then write unblocks it and child exits with the received byte.
 * pipe_read returns -EINTR on signal wakeup so child retries the read.
 * -------------------------------------------------------------------- */
static int test_tstp_pipe_blocked(void) {
  int fds[2];
  if (pipe(fds) < 0) return 0;

  pid_t child = fork();
  if (child == 0) {
    close(fds[1]);
    char buf[1];
    ssize_t n;
    /* Retry read on EINTR so stop/cont doesn't abort the wait. */
    do { n = read(fds[0], buf, 1); } while (n < 0 && errno == EINTR);
    close(fds[0]);
    _exit(n == 1 ? buf[0] : 1);
  }
  close(fds[0]);

  for (int i = 0; i < 3; i++) sched_yield();
  kill(child, SIGTSTP);

  int status;
  pid_t r = 0;
  for (int i = 0; i < 50; i++) {
    sched_yield();
    r = waitpid(child, &status, WUNTRACED | WNOHANG);
    if (r == child) break;
  }
  if (r != child || !WIFSTOPPED(status)) {
    close(fds[1]); kill(child, SIGKILL); waitpid(child, NULL, 0); return 0;
  }

  /* Resume then write to unblock the pipe read. */
  kill(child, SIGCONT);
  char val = 42;
  write(fds[1], &val, 1);
  close(fds[1]);

  r = waitpid(child, &status, 0);
  return r == child && WIFEXITED(status) && WEXITSTATUS(status) == 42;
}

/* -----------------------------------------------------------------------
 * Test 12: Stress — interleaved SIGTSTP/SIGCONT with concurrent children.
 * -------------------------------------------------------------------- */
static int test_tstp_interleaved_stress(void) {
#define N_IL 6
  pid_t kids[N_IL];
  for (int i = 0; i < N_IL; i++) {
    kids[i] = fork();
    if (kids[i] == 0) {
      while (1) sched_yield();
    }
  }

  /* Stop odd-indexed, leave even running, then swap. */
  for (int round = 0; round < 4; round++) {
    for (int i = 0; i < 3; i++) sched_yield();
    for (int i = 0; i < N_IL; i++) {
      if (i % 2 == round % 2)
        kill(kids[i], SIGTSTP);
      else
        kill(kids[i], SIGCONT);
    }
  }

  /* Resume all then kill. */
  for (int i = 0; i < N_IL; i++) kill(kids[i], SIGCONT);
  for (int i = 0; i < 3; i++) sched_yield();

  int pass = 1;
  for (int i = 0; i < N_IL; i++) {
    kill(kids[i], SIGKILL);
    int status;
    pid_t r = waitpid(kids[i], &status, 0);
    if (r != kids[i] || !WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL)
      pass = 0;
  }
  return pass;
#undef N_IL
}

/* -----------------------------------------------------------------------
 * Test 13: Race — SIGTSTP sent immediately after fork before parent
 * has had a chance to set tcsetpgrp. Child should still be stoppable.
 * -------------------------------------------------------------------- */
static int test_tstp_immediate_after_fork(void) {
  pid_t child = fork();
  if (child == 0) {
    setpgid(0, 0);
    while (1) sched_yield();
  }
  /* No sched_yield — send SIGTSTP as fast as possible after fork. */
  setpgid(child, child);
  kill(child, SIGTSTP);

  int status;
  pid_t r = 0;
  for (int i = 0; i < 100; i++) {
    sched_yield();
    r = waitpid(child, &status, WUNTRACED | WNOHANG);
    if (r == child) break;
  }
  if (r != child || !WIFSTOPPED(status)) {
    kill(child, SIGKILL); waitpid(child, NULL, 0); return 0;
  }
  kill(child, SIGKILL);
  waitpid(child, NULL, 0);
  return 1;
}

/* -----------------------------------------------------------------------
 * Runner
 * -------------------------------------------------------------------- */
typedef int (*test_fn)(void);

static struct { const char *name; test_fn fn; } tests[] = {
  { "test_tstp_wuntraced",         test_tstp_wuntraced         },
  { "test_tstp_resume_exit",       test_tstp_resume_exit       },
  { "test_tstp_pgid",              test_tstp_pgid              },
  { "test_tstp_multi_cycle",       test_tstp_multi_cycle       },
  { "test_sigstop_wuntraced",      test_sigstop_wuntraced      },
  { "test_tstp_stress",            test_tstp_stress            },
  { "test_sigcont_noop",           test_sigcont_noop           },
  { "test_stop_reported_once",     test_stop_reported_once     },
  { "test_sigkill_wakes_stopped",  test_sigkill_wakes_stopped  },
  { "test_tstp_ignored",           test_tstp_ignored           },
  { "test_tstp_pipe_blocked",      test_tstp_pipe_blocked      },
  { "test_tstp_interleaved_stress",   test_tstp_interleaved_stress   },
  { "test_tstp_immediate_after_fork", test_tstp_immediate_after_fork },
};

int main(void) {
  printf("=== job control tests (%d runs each) ===\n", N_RUNS);
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
