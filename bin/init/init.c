#include <stdio.h>
#include <unistd.h>
#include <types.h>
#include <stddef.h>

int main() {
  printf("Init process starting...\n");

  pid_t pid = fork();
  if (pid == 0) {
    printf("Child: Executing /bin/sh with argv and envp\n");
    char *argv[] = {
      "/bin/sh",
      "-c",
      "echo Hello from shell!",
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
  } else {
    int wstatus;
    wait(&wstatus);
  }
  return 0;
}
