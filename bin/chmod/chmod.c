#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(void) {
  fprintf(stderr, "Usage: chmod <octal-mode> <file>\n");
}

int main(int argc, char **argv) {
  if (argc != 3) {
    usage();
    return 1;
  }

  unsigned int mode = 0;
  const char *s = argv[1];
  while (*s >= '0' && *s <= '7') {
    mode = mode * 8 + (unsigned int)(*s - '0');
    s++;
  }
  if (*s != '\0') {
    fprintf(stderr, "chmod: invalid mode '%s'\n", argv[1]);
    return 1;
  }

  if (chmod(argv[2], mode) < 0) {
    fprintf(stderr, "chmod: cannot change permissions of '%s'\n", argv[2]);
    return 1;
  }
  return 0;
}
