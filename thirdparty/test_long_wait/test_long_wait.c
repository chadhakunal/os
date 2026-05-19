#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

static int test_passed = 0;
static int test_failed = 0;

static volatile int signal_received = 0;
static volatile int signal_number   = 0;

static void test_result(const char *name, int passed) {
  if (passed) {
    printf("  [PASS] %s\n", name);
    test_passed++;
  } else {
    printf("  [FAIL] %s\n", name);
    test_failed++;
  }
}

static void handler(int sig) {
  signal_received = 1;
  signal_number   = sig;
}

/*
 * Parent sleeps for 30 seconds. Child waits 2 seconds then sends SIGUSR1.
 * Tests that a signal correctly interrupts a long sleep and that the
 * process resumes normally afterwards.
 */
static void test_signal_interrupts_long_sleep(void) {
  signal_received = 0;
  signal_number   = 0;

  struct sigaction sa = {0};
  sa.sa_handler = handler;
  sigaction(SIGUSR1, &sa, NULL);

  pid_t parent_pid = getpid();
  pid_t child = fork();

  if (child == 0) {
    sleep(2);
    kill(parent_pid, SIGUSR1);
    exit(0);
  }

  printf("  sleeping for up to 30 seconds, expecting signal after ~2s\n");
  sleep(30);

  int status;
  waitpid(child, &status, 0);

  test_result("SIGUSR1 wakes process from long sleep", signal_received == 1);
  test_result("correct signal number (SIGUSR1)",        signal_number == SIGUSR1);
}

/*
 * Parent sleeps for 30 seconds. Child sends SIGUSR2 after 2 seconds.
 * After waking, parent sleeps again for 30 more seconds (should complete
 * quickly since no more signals). Tests that execution continues correctly
 * after being interrupted.
 */
static void test_continue_after_interrupted_sleep(void) {
  signal_received = 0;
  signal_number   = 0;

  struct sigaction sa = {0};
  sa.sa_handler = handler;
  sigaction(SIGUSR2, &sa, NULL);

  pid_t parent_pid = getpid();
  pid_t child = fork();

  if (child == 0) {
    sleep(2);
    kill(parent_pid, SIGUSR2);
    exit(0);
  }

  printf("  first sleep (30s, interrupted after ~2s by child)\n");
  sleep(30);

  int status;
  waitpid(child, &status, 0);

  int received_first = signal_received;

  printf("  second sleep (1s, no signal expected)\n");
  signal_received = 0;
  sleep(1);

  test_result("signal wakes first sleep",              received_first == 1);
  test_result("second sleep completes without signal", signal_received == 0);
}

int main(void) {
  printf("\n=== Long Wait / Sleep Interrupt Tests ===\n\n");

  test_signal_interrupts_long_sleep();
  test_continue_after_interrupted_sleep();

  printf("\n=== Test Summary ===\n");
  printf("Passed: %d\n", test_passed);
  printf("Failed: %d\n", test_failed);
  printf("\n");

  return test_failed > 0 ? 1 : 0;
}
