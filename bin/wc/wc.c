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

static void print(long l, long w, long b, const char *name,
                  int show_l, int show_w, int show_c) {
  if (show_l) printf("%7ld", l);
  if (show_w) printf("%7ld", w);
  if (show_c) printf("%7ld", b);
  if (name)   printf(" %s", name);
  printf("\n");
}

int main(int argc, char *argv[]) {
  int show_l = 0, show_w = 0, show_c = 0;
  int first = 1;

  /* Parse flags: -l -w -c (individually or combined e.g. -lw) */
  for (int i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
    for (int j = 1; argv[i][j]; j++) {
      if (argv[i][j] == 'l') show_l = 1;
      else if (argv[i][j] == 'w') show_w = 1;
      else if (argv[i][j] == 'c') show_c = 1;
    }
    first = i + 1;
  }

  /* Default: show all three */
  if (!show_l && !show_w && !show_c)
    show_l = show_w = show_c = 1;

  if (first >= argc) {
    long l = 0, w = 0, b = 0;
    count(0, &l, &w, &b);
    print(l, w, b, NULL, show_l, show_w, show_c);
    return 0;
  }

  long tl = 0, tw = 0, tb = 0;
  int err = 0;

  for (int i = first; i < argc; i++) {
    int fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "wc: %s: cannot open\n", argv[i]);
      err = 1;
      continue;
    }
    long l = 0, w = 0, b = 0;
    count(fd, &l, &w, &b);
    close(fd);
    print(l, w, b, argv[i], show_l, show_w, show_c);
    tl += l; tw += w; tb += b;
  }

  if (argc - first > 1)
    print(tl, tw, tb, "total", show_l, show_w, show_c);

  return err;
}
