#include <unistd.h>
#include <signal.h>
#include <stdio.h>

/* -----------------------------------------------------------------------
 * Test 1: SIGKILL terminates a child (existing test, kept as-is)
 * -------------------------------------------------------------------- */
static void test_sigkill(void) {
  printf("test_sigkill: START\n");

  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }

  for (int i = 0; i < 3; i++) sched_yield();

  kill(child, SIGKILL);

  int status;
  pid_t waited = waitpid(child, &status, 0);
  if (waited == child && status == 137)
    printf("test_sigkill: PASS (exit status %d)\n", status);
  else
    printf("test_sigkill: FAIL (waited=%d status=%d)\n", waited, status);
}

/* -----------------------------------------------------------------------
 * Test 2: SIGSTOP suspends a child, SIGCONT resumes it.
 * The child spins then exits. We stop it mid-spin, yield many times
 * (the child cannot make progress while stopped), then SIGCONT and
 * verify it exits cleanly with status 0.
 * -------------------------------------------------------------------- */
static void test_sigstop_sigcont(void) {
  printf("test_sigstop_sigcont: START\n");

  pid_t child = fork();
  if (child == 0) {
    for (volatile int i = 0; i < 5000000; i++);
    _exit(0);
  }

  for (int i = 0; i < 5; i++) sched_yield();
  printf("test_sigstop_sigcont: sending SIGSTOP to child %d\n", child);
  kill(child, SIGSTOP);

  printf("test_sigstop_sigcont: yielding while child is stopped...\n");
  for (int i = 0; i < 20; i++) sched_yield();
  printf("test_sigstop_sigcont: parent still running (child suspended)\n");

  printf("test_sigstop_sigcont: sending SIGCONT to child %d\n", child);
  kill(child, SIGCONT);

  int status;
  pid_t r = waitpid(child, &status, 0);
  if (r == child && status == 0)
    printf("test_sigstop_sigcont: PASS - child exited cleanly after SIGCONT\n");
  else
    printf("test_sigstop_sigcont: FAIL - waited=%d status=%d\n", r, status);
}

/* -----------------------------------------------------------------------
 * Test 3: SIGSTOP then SIGKILL wakes and kills a stopped process
 * -------------------------------------------------------------------- */
static void test_sigkill_stopped(void) {
  printf("test_sigkill_stopped: START\n");

  pid_t child = fork();
  if (child == 0) {
    while (1) sched_yield();
  }

  for (int i = 0; i < 3; i++) sched_yield();

  kill(child, SIGSTOP);
  for (int i = 0; i < 5; i++) sched_yield();

  /* SIGKILL must wake and kill even a stopped process. */
  kill(child, SIGKILL);

  int status;
  pid_t r = waitpid(child, &status, 0);
  if (r == child && status == 137)
    printf("test_sigkill_stopped: PASS (status=%d)\n", status);
  else
    printf("test_sigkill_stopped: FAIL (waited=%d status=%d)\n", r, status);
}

/* -----------------------------------------------------------------------
 * Test 4: Stress test — rapid stop/cont on multiple children to hit
 * scheduler races (timer firing mid context-switch).
 * -------------------------------------------------------------------- */
static void test_stress_stop_cont(void) {
  printf("test_stress_stop_cont: START\n");

#define N_STRESS 4
  pid_t kids[N_STRESS];

  for (int i = 0; i < N_STRESS; i++) {
    kids[i] = fork();
    if (kids[i] == 0) {
      for (volatile long j = 0; j < 20000000L; j++);
      _exit(0);
    }
  }

  /* Rapidly stop and cont each child several times. */
  for (int round = 0; round < 8; round++) {
    for (int i = 0; i < N_STRESS; i++)
      kill(kids[i], SIGSTOP);
    for (int i = 0; i < N_STRESS; i++)
      kill(kids[i], SIGCONT);
  }

  int pass = 1;
  for (int i = 0; i < N_STRESS; i++) {
    int status;
    pid_t r = waitpid(kids[i], &status, 0);
    if (r != kids[i] || status != 0) {
      printf("test_stress_stop_cont: FAIL child %d (waited=%d status=%d)\n",
             kids[i], r, status);
      pass = 0;
    }
  }
  if (pass)
    printf("test_stress_stop_cont: PASS\n");
#undef N_STRESS
}

int main(void) {
  printf("=== signal tests ===\n");
  test_sigkill();
  test_sigstop_sigcont();
  test_sigkill_stopped();
  test_stress_stop_cont();
  printf("=== done ===\n");
  return 0;
}
