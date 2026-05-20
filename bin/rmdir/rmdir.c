#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: rmdir <dir>...\n");
    return 1;
  }
  int ret = 0;
  for (int i = 1; i < argc; i++) {
    if (rmdir(argv[i]) < 0) {
      fprintf(stderr, "rmdir: failed to remove '%s'\n", argv[i]);
      ret = 1;
    }
  }
  return ret;
}
