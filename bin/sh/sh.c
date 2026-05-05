#include <stdio.h>
#include <unistd.h>
#include <types.h>
#include <stddef.h>

int main(int argc, char **argv, char **envp) {
  printf("Shell started!\n");
  printf("argc = %d\n", argc);

  printf("Arguments:\n");
  for (int i = 0; i < argc; i++) {
    printf("  argv[%d] = %s\n", i, argv[i]);
  }

  printf("Environment:\n");
  printf("envp pointer = %p\n", envp);
  if (envp != NULL) {
    printf("envp[0] = %p\n", envp[0]);
    for (int i = 0; envp[i] != NULL; i++) {
      printf("  envp[%d] = %s\n", i, envp[i]);
    }
  }

  char buf[1024];
  while (true) {
    printf("$ ");
    read(0, buf, sizeof(buf));
    printf("Shell: %s", buf);
  }

  return 0;
}
