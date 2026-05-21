#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int flag_r = 0;
static int flag_n = 0;
static int flag_u = 0;
static int key    = 0; /* 1-based field for -k, 0 = whole line */

#define MAX_LINES 16384
#define MAX_LINE  1024

static char  line_buf[MAX_LINES][MAX_LINE];
static char *lines[MAX_LINES];
static int   nlines = 0;

static const char *get_key(const char *line) {
  if (key == 0) return line;
  int k = key;
  const char *p = line;
  /* skip leading whitespace */
  while (*p == ' ' || *p == '\t') p++;
  while (--k > 0) {
    /* skip non-whitespace (current field) */
    while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    /* skip whitespace between fields */
    while (*p == ' ' || *p == '\t') p++;
  }
  return p;
}

static int cmp(const void *a, const void *b) {
  const char *ka = get_key(*(const char **)a);
  const char *kb = get_key(*(const char **)b);
  int r;
  if (flag_n) {
    long va = atol(ka);
    long vb = atol(kb);
    r = (va > vb) - (va < vb);
  } else {
    r = strcmp(ka, kb);
  }
  return flag_r ? -r : r;
}

static int read_lines(int fd) {
  char buf[4096];
  int  partial = 0;
  char line[MAX_LINE];
  int  linelen = 0;
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
  /* last line without trailing newline */
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
      case 'k':
        if (argv[i][j + 1]) {
          key = atoi(&argv[i][j + 1]);
          j += strlen(&argv[i][j + 1]);
        } else if (i + 1 < argc) {
          key = atoi(argv[++i]);
          j = strlen(argv[i]) - 1; /* end inner loop */
        }
        if (key < 1) key = 1;
        break;
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

  qsort(lines, nlines, sizeof(char *), cmp);

  const char *prev = NULL;
  for (int j = 0; j < nlines; j++) {
    if (flag_u && prev != NULL) {
      const char *ka = get_key(prev);
      const char *kb = get_key(lines[j]);
      int eq = flag_n ? (atol(ka) == atol(kb)) : (strcmp(ka, kb) == 0);
      if (eq) continue;
    }
    fputs(lines[j], stdout);
    prev = lines[j];
  }

  return 0;
}
