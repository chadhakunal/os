#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

int test_passed = 0;
int test_failed = 0;

volatile int signal_received = 0;
volatile int signal_number = 0;

void test_result(const char *name, int passed) {
  if (passed) {
    printf("  [PASS] %s\n", name);
    test_passed++;
  } else {
    printf("  [FAIL] %s\n", name);
    test_failed++;
  }
}

void signal_handler(int sig) {
  signal_received = 1;
  signal_number = sig;
}

void test_signal_handler_registration() {
  // Test that we can register a signal handler
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;

  int ret = sigaction(SIGUSR1, &sa, NULL);
  test_result("sigaction registers handler", ret == 0);
}

void test_signal_delivery() {
  // Reset flags
  signal_received = 0;
  signal_number = 0;

  // Register handler
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigaction(SIGUSR1, &sa, NULL);

  // Send signal to self
  pid_t my_pid = getpid();
  kill(my_pid, SIGUSR1);

  // Give signal time to be delivered (busy wait a bit)
  for (volatile int i = 0; i < 1000; i++);

  test_result("signal is delivered to process", signal_received == 1);
  test_result("correct signal number received", signal_number == SIGUSR1);
}

void test_signal_between_processes() {
  signal_received = 0;
  signal_number = 0;

  // Register handler in parent
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigaction(SIGUSR2, &sa, NULL);

  pid_t parent_pid = getpid();
  pid_t pid = fork();

  if (pid == 0) {
    // Child: send signal to parent
    kill(parent_pid, SIGUSR2);
    exit(0);
  } else if (pid > 0) {
    // Parent: wait for signal
    int status;
    wait(&status);

    // Check if signal was received
    test_result("signal sent from child to parent", signal_received == 1);
    test_result("correct signal (SIGUSR2) received", signal_number == SIGUSR2);
  } else {
    test_result("fork for signal test", 0);
  }
}

void test_signal_default_behavior() {
  // Test that default signal handler works (child should be killed)
  pid_t pid = fork();

  if (pid == 0) {
    // Child: reset SIGUSR1 to default handler
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    // Send signal to self (should terminate)
    kill(getpid(), SIGUSR1);

    // Should not reach here
    exit(99);
  } else if (pid > 0) {
    int status;
    wait(&status);

    // Child should not have exited normally with 99
    test_result("default signal handler terminates process", status != 99);
  } else {
    test_result("fork for default handler test", 0);
  }
}

void test_signal_ignore() {
  signal_received = 0;

  // Set SIGUSR1 to be ignored
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sigaction(SIGUSR1, &sa, NULL);

  // Send signal to self
  kill(getpid(), SIGUSR1);

  // Wait a bit
  for (volatile int i = 0; i < 1000; i++);

  test_result("SIG_IGN ignores signal", signal_received == 0);

  // Restore handler for other tests
  sa.sa_handler = signal_handler;
  sigaction(SIGUSR1, &sa, NULL);
}

void test_multiple_signals() {
  signal_received = 0;
  int signal_count = 0;

  // Handler that counts signals
  struct sigaction sa;
  sa.sa_handler = (void (*)(int))((void *)&signal_count); // Hacky, but we'll increment manually
  sa.sa_flags = 0;

  // Actually, let's just send multiple signals and check we can receive them
  sa.sa_handler = signal_handler;
  sigaction(SIGUSR1, &sa, NULL);

  pid_t my_pid = getpid();

  // Send signal multiple times
  kill(my_pid, SIGUSR1);
  for (volatile int i = 0; i < 100; i++);

  int first_received = signal_received;

  signal_received = 0;
  kill(my_pid, SIGUSR1);
  for (volatile int i = 0; i < 100; i++);

  int second_received = signal_received;

  test_result("multiple signals can be delivered", first_received && second_received);
}

int main(int argc, char **argv) {
  printf("\n=== Signal System Tests ===\n\n");

  printf("Basic Signal Operations:\n");
  test_signal_handler_registration();
  test_signal_delivery();
  test_signal_ignore();

  printf("\nInter-Process Signals:\n");
  test_signal_between_processes();

  printf("\nSignal Handler Behavior:\n");
  test_signal_default_behavior();
  test_multiple_signals();

  printf("\n=== Test Summary ===\n");
  printf("Passed: %d\n", test_passed);
  printf("Failed: %d\n", test_failed);
  printf("\n");

  return test_failed > 0 ? 1 : 0;
}
