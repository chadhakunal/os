#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#define DEFAULT_LINES 10

int tail_lines(int fd, int num_lines) {
  off_t file_size = lseek(fd, 0, SEEK_END);
  if (file_size < 0) {
    fprintf(stderr, "tail: failed to seek to end\n");
    return 1;
  }

  if (file_size == 0) {
    return 0;
  }

  char buf[1];
  int lines_found = 0;
  off_t pos = file_size - 1;

  /* Skip a trailing newline so it doesn't count as an extra line boundary. */
  lseek(fd, pos, SEEK_SET);
  read(fd, buf, 1);
  if (buf[0] == '\n')
    pos--;

  while (pos >= 0 && lines_found < num_lines) {
    if (lseek(fd, pos, SEEK_SET) < 0) {
      fprintf(stderr, "tail: seek failed\n");
      return 1;
    }

    if (read(fd, buf, 1) != 1) {
      fprintf(stderr, "tail: read failed\n");
      return 1;
    }

    if (buf[0] == '\n') {
      lines_found++;
      if (lines_found == num_lines) {
        pos++;
        break;
      }
    }
    pos--;
  }

  if (pos < 0) {
    pos = 0;
  }

  if (lseek(fd, pos, SEEK_SET) < 0) {
    fprintf(stderr, "tail: seek failed\n");
    return 1;
  }

  char output_buf[256];
  ssize_t n;
  while ((n = read(fd, output_buf, sizeof(output_buf))) > 0) {
    write(1, output_buf, n);
  }

  return 0;
}

int tail_bytes(int fd, int num_bytes) {
  off_t file_size = lseek(fd, 0, SEEK_END);
  if (file_size < 0) {
    fprintf(stderr, "tail: failed to seek to end\n");
    return 1;
  }

  if (file_size == 0) {
    return 0;
  }

  off_t start_pos = 0;
  if (file_size > num_bytes) {
    start_pos = file_size - num_bytes;
  }

  if (lseek(fd, start_pos, SEEK_SET) < 0) {
    fprintf(stderr, "tail: seek failed\n");
    return 1;
  }

  char buf[256];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    write(1, buf, n);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  int use_bytes = 0;
  int num = DEFAULT_LINES;
  int opt;

  while ((opt = getopt(argc, argv, "c:n:")) != -1) {
    switch (opt) {
      case 'c':
        use_bytes = 1;
        num = atoi(optarg);
        if (num <= 0) {
          fprintf(stderr, "tail: invalid number of bytes: '%s'\n", optarg);
          return 1;
        }
        break;
      case 'n':
        num = atoi(optarg);
        if (num <= 0) {
          fprintf(stderr, "tail: invalid number of lines: '%s'\n", optarg);
          return 1;
        }
        break;
      case '?':
        fprintf(stderr, "Usage: tail [-c NUM] [-n NUM] <file>\n");
        return 1;
    }
  }

  if (optind >= argc) {
    /* No filename: read from stdin. Buffer all input, output last N lines. */
    static char ibuf[65536];
    ssize_t total = 0, n;
    while (total < (ssize_t)(sizeof(ibuf) - 1) &&
           (n = read(0, ibuf + total, sizeof(ibuf) - 1 - total)) > 0)
      total += n;
    ibuf[total] = '\0';

    if (use_bytes) {
      ssize_t start = total > num ? total - num : 0;
      write(1, ibuf + start, total - start);
    } else {
      /* find the start of the last `num` lines */
      int lines = 0;
      ssize_t pos = total;
      /* skip trailing newline */
      if (pos > 0 && ibuf[pos - 1] == '\n') pos--;
      while (pos > 0) {
        pos--;
        if (ibuf[pos] == '\n') {
          lines++;
          if (lines == num) { pos++; break; }
        }
      }
      write(1, ibuf + pos, total - pos);
    }
    return 0;
  }

  const char *filename = argv[optind];

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "tail: cannot open '%s'\n", filename);
    return 1;
  }

  int ret;
  if (use_bytes) {
    ret = tail_bytes(fd, num);
  } else {
    ret = tail_lines(fd, num);
  }

  close(fd);
  return ret;
}
