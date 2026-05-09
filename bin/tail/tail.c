#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#define DEFAULT_LINES 10

int tail_lines(int fd, int num_lines) {
  off_t file_size = lseek(fd, 0, SEEK_END);
  if (file_size < 0) {
    printf("tail: failed to seek to end\n");
    return 1;
  }

  if (file_size == 0) {
    return 0;
  }

  char buf[1];
  int lines_found = 0;
  off_t pos = file_size - 1;

  while (pos >= 0 && lines_found < num_lines) {
    if (lseek(fd, pos, SEEK_SET) < 0) {
      printf("tail: seek failed\n");
      return 1;
    }

    if (read(fd, buf, 1) != 1) {
      printf("tail: read failed\n");
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
    printf("tail: seek failed\n");
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
    printf("tail: failed to seek to end\n");
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
    printf("tail: seek failed\n");
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

  while ((opt = getopt(argc, argv, "c:")) != -1) {
    switch (opt) {
      case 'c':
        use_bytes = 1;
        num = atoi(optarg);
        if (num <= 0) {
          printf("tail: invalid number of bytes: '%s'\n", optarg);
          return 1;
        }
        break;
      case '?':
        printf("Usage: tail [-c NUM] <file>\n");
        return 1;
    }
  }

  if (optind >= argc) {
    printf("Usage: tail [-c NUM] <file>\n");
    printf("  -c NUM    output last NUM bytes\n");
    printf("  default: output last 10 lines\n");
    return 1;
  }

  const char *filename = argv[optind];

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    printf("tail: cannot open '%s'\n", filename);
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
