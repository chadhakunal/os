#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: touch <file>...\n");
    return 1;
  }
  int ret = 0;
  for (int i = 1; i < argc; i++) {
    int fd = open(argv[i], O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
      printf("touch: failed to create %s\n", argv[i]);
      ret = 1;
      continue;
    }
    close(fd);
  }
  return ret;
}
