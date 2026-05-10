#include <unistd.h>
#include <signal.h>
#include <stdio.h>

int main(int argc, char **argv) {
  printf("testsignal: Starting signal test\n");

  pid_t child = fork();

  if (child == 0) {
    printf("testsignal: Child process (PID should be non-zero)\n");
    printf("testsignal: Child waiting to be killed...\n");

    while (1) {
      sched_yield();
    }

    printf("testsignal: Child should never reach here!\n");
    return 1;
  } else {
    printf("testsignal: Parent process, child PID = %d\n", child);
    printf("testsignal: Parent yielding to let child run...\n");

    for (int i = 0; i < 5; i++) {
      sched_yield();
    }

    printf("testsignal: Parent sending SIGKILL to child PID %d\n", child);
    int ret = kill(child, SIGKILL);
    printf("testsignal: kill() returned %d\n", ret);

    printf("testsignal: Parent waiting for child to exit...\n");
    int status;
    pid_t waited = waitpid(child, &status, 0);
    printf("testsignal: waitpid returned %d, status = %d\n", waited, status);

    if (status == 137) {
      printf("testsignal: SUCCESS - child killed by SIGKILL (exit status 128+9=137)\n");
    } else {
      printf("testsignal: FAIL - unexpected exit status %d\n", status);
    }

    return 0;
  }
}
