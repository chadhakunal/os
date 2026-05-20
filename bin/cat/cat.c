#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

static void cat_fd(int fd) {
  char buf[256];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0)
    write(1, buf, n);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    cat_fd(0);
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1] == '\0') {
      cat_fd(0);
      continue;
    }
    int fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "cat: %s: no such file or directory\n", argv[i]);
      return 1;
    }
    cat_fd(fd);
    close(fd);
  }

  return 0;
}
