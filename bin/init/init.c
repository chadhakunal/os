#include <stdio.h>
#include <unistd.h>
#include <types.h>
#include <stddef.h>

static pid_t shell_pid = -1;

static pid_t start_shell(void) {
  pid_t pid = fork();
  if (pid == 0) {
    char *argv[] = { "/bin/sh", NULL };
    char *envp[] = { "PATH=/bin", "HOME=/", "USER=root", NULL };
    execve("/bin/sh", argv, envp);
    printf("init: execve /bin/sh failed\n");
    return 1;
  }
  return pid;
}

int main(void) {
  printf("Init process starting...\n");

  // Run rc script synchronously
  printf("Running /etc/rc...\n");
  pid_t rc_pid = fork();
  if (rc_pid == 0) {
    char *argv[] = { "/bin/sh", "/etc/rc", NULL };
    char *envp[] = { "PATH=/bin", "HOME=/", "USER=root", NULL };
    execve("/bin/sh", argv, envp);
    printf("init: execve rc failed\n");
    return 1;
  } else if (rc_pid > 0) {
    int wstatus;
    waitpid(rc_pid, &wstatus, 0);
    printf("Init: rc completed with status %d\n", wstatus);
  }

  // Start interactive shell
  printf("Starting interactive shell...\n");
  shell_pid = start_shell();

  // Main init loop: reap any child, respawn shell if it exits
  while (1) {
    int wstatus;
    pid_t dead = waitpid(-1, &wstatus, 0);
    if (dead < 0)
      continue;

    printf("Init: child %d exited with status %d\n", dead, wstatus);

    if (dead == shell_pid) {
      printf("Init: shell exited, respawning...\n");
      shell_pid = start_shell();
    }
  }

  return 0;
}
