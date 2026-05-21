#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int flag_r = 0;
static int flag_n = 0;
static int flag_u = 0;

#define MAX_LINES 16384
#define MAX_LINE  1024

static char  line_buf[MAX_LINES][MAX_LINE];
static char *lines[MAX_LINES];
static int   nlines = 0;

static int line_cmp(const char *a, const char *b) {
  int r = flag_n ? (atol(a) > atol(b)) - (atol(a) < atol(b))
                 : strcmp(a, b);
  return flag_r ? -r : r;
}

static void do_sort(void) {
  for (int i = 1; i < nlines; i++) {
    char *key = lines[i];
    int j = i - 1;
    while (j >= 0 && line_cmp(lines[j], key) > 0) {
      lines[j + 1] = lines[j];
      j--;
    }
    lines[j + 1] = key;
  }
}

static int read_lines(int fd) {
  char buf[4096];
  char line[MAX_LINE];
  int  linelen = 0;
  int  partial = 0;
  ssize_t n;

  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (ssize_t i = 0; i < n; i++) {
      char c = buf[i];
      if (linelen < MAX_LINE - 1)
        line[linelen++] = c;
      if (c == '\n') {
        line[linelen] = '\0';
        if (nlines >= MAX_LINES) {
          fprintf(stderr, "sort: too many lines (max %d)\n", MAX_LINES);
          return -1;
        }
        memcpy(line_buf[nlines], line, linelen + 1);
        lines[nlines] = line_buf[nlines];
        nlines++;
        linelen = 0;
        partial = 0;
      } else {
        partial = 1;
      }
    }
  }
  if (partial && linelen > 0) {
    line[linelen++] = '\n';
    line[linelen]   = '\0';
    if (nlines < MAX_LINES) {
      memcpy(line_buf[nlines], line, linelen + 1);
      lines[nlines] = line_buf[nlines];
      nlines++;
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  int i;
  for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
    if (argv[i][1] == '-' && argv[i][2] == '\0') { i++; break; }
    for (int j = 1; argv[i][j]; j++) {
      switch (argv[i][j]) {
      case 'r': flag_r = 1; break;
      case 'n': flag_n = 1; break;
      case 'u': flag_u = 1; break;
      default:
        fprintf(stderr, "sort: unknown flag -%c\n", argv[i][j]);
        return 2;
      }
    }
  }

  int nfiles = argc - i;
  if (nfiles == 0) {
    if (read_lines(0) < 0) return 1;
  } else {
    for (int f = i; f < argc; f++) {
      int fd = open(argv[f], O_RDONLY);
      if (fd < 0) {
        fprintf(stderr, "sort: %s: cannot open\n", argv[f]);
        return 1;
      }
      int r = read_lines(fd);
      close(fd);
      if (r < 0) return 1;
    }
  }

  do_sort();

  const char *prev = NULL;
  for (int j = 0; j < nlines; j++) {
    if (flag_u && prev != NULL && strcmp(lines[j], prev) == 0)
      continue;
    fputs(lines[j], stdout);
    prev = lines[j];
  }

  return 0;
}
