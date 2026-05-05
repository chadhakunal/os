#include <stdio.h>
#include <unistd.h>
#include <types.h>
#include <stddef.h>
#include <fcntl.h>

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

  printf("\nTesting open syscall:\n");
  int fd = open("/etc/rc", O_RDONLY);
  printf("open(\"/etc/rc\", O_RDONLY) = %d\n", fd);

  if (fd >= 0) {
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    printf("read() returned %d bytes\n", (int)n);
    if (n > 0) {
      buf[n] = '\0';
      printf("Contents:\n%s\n", buf);
    }
  }

  char buf[1024];
  while (true) {
    printf("$ ");
    ssize_t n = read(0, buf, sizeof(buf) - 1);
    if (n > 0) {
      buf[n] = '\0';
      printf("Shell: %s", buf);
    }
  }

  return 0;
}
