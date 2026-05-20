#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int sig = SIGTERM;
  int i = 1;

  if (argc < 2) {
    fprintf(stderr, "usage: kill [-<sig>] <pid>...\n");
    return 1;
  }

  if (argv[1][0] == '-') {
    sig = atoi(&argv[1][1]);
    if (sig <= 0) {
      fprintf(stderr, "kill: invalid signal: %s\n", argv[1]);
      return 1;
    }
    i = 2;
  }

  if (i >= argc) {
    fprintf(stderr, "usage: kill [-<sig>] <pid>...\n");
    return 1;
  }

  int ret = 0;
  for (; i < argc; i++) {
    pid_t pid = (pid_t)atoi(argv[i]);
    if (kill(pid, sig) < 0) {
      perror("kill");
      ret = 1;
    }
  }
  return ret;
}
