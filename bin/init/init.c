#include <stdio.h>
#include <unistd.h>
#include <types.h>
#include <stddef.h>

int main() {
  printf("Init process starting...\n");

  // Loop forever - init should never exit
  while (1) {
    pid_t pid = fork();
    if (pid == 0) {
      printf("Child: Executing /bin/sh\n");
      char *argv[] = {
        "/bin/sh",
        NULL
      };

      char *envp[] = {
        "PATH=/bin",
        "HOME=/",
        "USER=root",
        NULL
      };

      execve("/bin/sh", argv, envp);

      printf("Child: execve failed!\n");
      return 1;
    } else if (pid > 0) {
      int wstatus;
      wait(&wstatus);
      printf("Init: shell exited, restarting...\n");
    } else {
      printf("Init: fork failed!\n");
      // If fork fails, sleep a bit and try again
      sleep(1);
    }
  }

  return 0;
}
