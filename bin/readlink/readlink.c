#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: readlink <path>\n");
    return 1;
  }

  char buf[256];
  ssize_t n = readlink(argv[1], buf, sizeof(buf) - 1);
  if (n < 0) {
    fprintf(stderr, "readlink: %s: not a symlink or does not exist\n", argv[1]);
    return 1;
  }
  buf[n] = '\0';
  printf("%s\n", buf);
  return 0;
}
