#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void count(int fd, long *lines, long *words, long *bytes) {
  char buf[512];
  int in_word = 0;
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    *bytes += n;
    for (ssize_t i = 0; i < n; i++) {
      char c = buf[i];
      if (c == '\n') (*lines)++;
      int space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
      if (!space && !in_word) { (*words)++; in_word = 1; }
      else if (space)           in_word = 0;
    }
  }
}

static void print(long l, long w, long b, const char *name) {
  printf("%7ld %7ld %7ld", l, w, b);
  if (name) printf(" %s", name);
  printf("\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    long l = 0, w = 0, b = 0;
    count(0, &l, &w, &b);
    print(l, w, b, NULL);
    return 0;
  }

  long tl = 0, tw = 0, tb = 0;
  int err = 0;

  for (int i = 1; i < argc; i++) {
    int fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "wc: %s: cannot open\n", argv[i]);
      err = 1;
      continue;
    }
    long l = 0, w = 0, b = 0;
    count(fd, &l, &w, &b);
    close(fd);
    print(l, w, b, argv[i]);
    tl += l; tw += w; tb += b;
  }

  if (argc > 2)
    print(tl, tw, tb, "total");

  return err;
}
