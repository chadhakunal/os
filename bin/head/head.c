#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define DEFAULT_LINES 10

static int head_fd(int fd, int nlines) {
  char buf[512];
  int lines = 0;
  ssize_t n;
  while (lines < nlines && (n = read(fd, buf, sizeof(buf))) > 0) {
    for (ssize_t i = 0; i < n; i++) {
      write(1, &buf[i], 1);
      if (buf[i] == '\n') {
        lines++;
        if (lines >= nlines)
          return 0;
      }
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  int nlines = DEFAULT_LINES;
  int first = 1;

  if (argc >= 3 && argv[1][0] == '-' && argv[1][1] == 'n') {
    nlines = atoi(argv[2]);
    if (nlines <= 0) {
      fprintf(stderr, "head: invalid number of lines: '%s'\n", argv[2]);
      return 1;
    }
    first = 3;
  } else if (argc >= 2 && argv[1][0] == '-' && argv[1][1] != '\0') {
    /* Support -N shorthand */
    nlines = atoi(argv[1] + 1);
    if (nlines <= 0) {
      fprintf(stderr, "head: invalid option: '%s'\n", argv[1]);
      return 1;
    }
    first = 2;
  }

  if (first >= argc) {
    return head_fd(0, nlines);
  }

  int ret = 0;
  int multiple = (argc - first > 1);
  for (int i = first; i < argc; i++) {
    int fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "head: cannot open '%s'\n", argv[i]);
      ret = 1;
      continue;
    }
    if (multiple)
      printf("==> %s <==\n", argv[i]);
    head_fd(fd, nlines);
    if (multiple && i < argc - 1)
      putchar('\n');
    close(fd);
  }
  return ret;
}
