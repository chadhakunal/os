#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

extern char **environ;

int system(const char *command) {
  pid_t pid;
  int status;

  if (!command)
    return 0;

  pid = fork();
  if (pid < 0)
    return -1;

  if (pid == 0) {
    char *argv[] = { "sh", "-c", (char *)command, NULL };
    execve("/bin/sh", argv, environ);
    _exit(127);
  }

  if (waitpid(pid, &status, 0) < 0)
    return -1;

  return status;
}
