#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>

#define N_RUNS 100

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
    for (volatile long i = 0; i < 50000000L; i++);
    _exit(0);
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

  int status;
  pid_t r = waitpid(child, &status, 0);
  return r == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
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
 * Runner
 * -------------------------------------------------------------------- */
typedef int (*test_fn)(void);

static struct { const char *name; test_fn fn; } tests[] = {
  { "test_tstp_wuntraced",    test_tstp_wuntraced    },
  { "test_tstp_resume_exit",  test_tstp_resume_exit  },
  { "test_tstp_pgid",         test_tstp_pgid         },
  { "test_tstp_multi_cycle",  test_tstp_multi_cycle  },
  { "test_sigstop_wuntraced", test_sigstop_wuntraced },
  { "test_tstp_stress",       test_tstp_stress       },
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
