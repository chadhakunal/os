#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: mount <source> <target> <fstype>\n");
    return 1;
  }
  if (mount(argv[1], argv[2], argv[3], 0, NULL) < 0) {
    perror("mount");
    return 1;
  }
  return 0;
}
