#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int main(void) {
  pid_t pid = fork();

  if (pid == 0) {
    /* Child: allocate beyond 64KB stack limit to trigger SIGSEGV */
    volatile char buf[65536 + 4096];
    for (int i = 0; i < 65536 + 4096; i++)
      buf[i] = (char)i;
    printf("ERROR: should have received SIGSEGV\n");
    return 0;
  }

  int status;
  waitpid(pid, &status, 0);

  if (status == SIGSEGV) {
    printf("Stack overflow test passed: child killed by SIGSEGV\n");
    return 0;
  }

  printf("Stack overflow test FAILED: exit status=%d (expected SIGSEGV=%d)\n",
         status, SIGSEGV);
  return 1;
}
