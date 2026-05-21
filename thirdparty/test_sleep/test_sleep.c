#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

static int test_passed = 0;
static int test_failed = 0;

static void test_result(const char *name, int passed) {
  if (passed) {
    printf("  [PASS] %s\n", name);
    test_passed++;
  } else {
    printf("  [FAIL] %s\n", name);
    test_failed++;
  }
}

/* ------------------------------------------------------------------ */
/* Basic sleep/nanosleep/usleep                                        */
/* ------------------------------------------------------------------ */

static void test_sleep_returns(void) {
  int ret = sleep(1);
  test_result("sleep(1) returns 0 when not interrupted", ret == 0);
}

static void test_nanosleep_returns(void) {
  struct timespec req = { .tv_sec = 0, .tv_nsec = 50000000 }; /* 50 ms */
  struct timespec rem = { 0, 0 };
  int ret = nanosleep(&req, &rem);
  test_result("nanosleep(50ms) returns 0", ret == 0);
}

static void test_usleep_returns(void) {
  int ret = usleep(50000); /* 50 ms */
  test_result("usleep(50000) returns 0", ret == 0);
}

/* ------------------------------------------------------------------ */
/* Signal delivery during sleep                                        */
/* ------------------------------------------------------------------ */

static volatile int signal_received = 0;
static volatile int signal_number   = 0;

static void wakeup_handler(int sig) {
  signal_received = 1;
  signal_number   = sig;
}

/*
 * Parent sleeps for a long time; child sends SIGUSR1 shortly after.
 * We verify:
 *   1. The handler ran (signal_received == 1).
 *   2. sleep() returned the remaining seconds (> 0), indicating early wakeup.
 */
static void test_signal_interrupts_sleep(void) {
  signal_received = 0;
  signal_number   = 0;

  struct sigaction sa;
  sa.sa_handler = wakeup_handler;
  sa.sa_flags   = 0;
  sigaction(SIGUSR1, &sa, NULL);

  pid_t parent_pid = getpid();
  pid_t child = fork();

  if (child == 0) {
    /* Child waits briefly then signals the parent. */
    usleep(200000); /* 200 ms */
    kill(parent_pid, SIGUSR1);
    exit(0);
  }

  /* Parent sleeps for much longer than child's delay. */
  unsigned int remaining = sleep(10);

  int status;
  waitpid(child, &status, 0);

  test_result("SIGUSR1 handler ran during sleep",    signal_received == 1);
  test_result("correct signal number (SIGUSR1)",     signal_number == SIGUSR1);
  test_result("sleep returned remaining time > 0",   remaining > 0);
}

/*
 * Same idea but using nanosleep: the rem timespec should be populated
 * with the remaining time when interrupted.
 */
static void test_signal_interrupts_nanosleep(void) {
  signal_received = 0;

  struct sigaction sa;
  sa.sa_handler = wakeup_handler;
  sa.sa_flags   = 0;
  sigaction(SIGUSR2, &sa, NULL);

  pid_t parent_pid = getpid();
  pid_t child = fork();

  if (child == 0) {
    usleep(200000); /* 200 ms */
    kill(parent_pid, SIGUSR2);
    exit(0);
  }

  struct timespec req = { .tv_sec = 10, .tv_nsec = 0 };
  struct timespec rem = { 0, 0 };
  int ret = nanosleep(&req, &rem);

  int status;
  waitpid(child, &status, 0);

  test_result("SIGUSR2 handler ran during nanosleep", signal_received == 1);
  /* On interruption nanosleep returns -1; rem holds remaining time. */
  test_result("nanosleep rem.tv_sec > 0 after interruption", rem.tv_sec > 0);
  (void)ret;
}

/* ------------------------------------------------------------------ */

int main(void) {
  printf("\n=== Sleep Syscall Tests ===\n\n");

  printf("Basic sleep functions:\n");
  test_sleep_returns();
  test_nanosleep_returns();
  test_usleep_returns();

  printf("\nSignal delivery during sleep:\n");
  test_signal_interrupts_sleep();
  test_signal_interrupts_nanosleep();

  printf("\n=== Test Summary ===\n");
  printf("Passed: %d\n", test_passed);
  printf("Failed: %d\n", test_failed);
  printf("\n");

  return test_failed > 0 ? 1 : 0;
}
