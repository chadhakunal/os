#include <stdio.h>
#include <unistd.h>
#include <types.h>
#include <stddef.h>

int main() {
  printf("Init process starting...\n");

  // Run the rc initialization script first
  printf("Running /etc/rc...\n");
  pid_t rc_pid = fork();
  if (rc_pid == 0) {
    char *argv[] = {
      "/bin/sh",
      "/etc/rc",
      NULL
    };

    char *envp[] = {
      "PATH=/bin",
      "HOME=/",
      "USER=root",
      NULL
    };

    execve("/bin/sh", argv, envp);

    printf("Init: failed to execute rc script\n");
    return 1;
  } else if (rc_pid > 0) {
    int wstatus;
    wait(&wstatus);
    printf("Init: rc script completed with status %d\n", wstatus);
  } else {
    printf("Init: fork failed for rc script\n");
  }

  // Now start the interactive shell and respawn it if it exits
  printf("Starting interactive shell...\n");
  while (1) {
    pid_t pid = fork();
    if (pid == 0) {
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
    }
  }

  return 0;
}
