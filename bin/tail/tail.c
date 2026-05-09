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
  printf("tail_bytes: file_size=%ld\n", file_size);
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
  printf("tail_bytes: num_bytes=%d, start_pos=%ld\n", num_bytes, start_pos);

  off_t seek_result = lseek(fd, start_pos, SEEK_SET);
  printf("tail_bytes: lseek returned %ld\n", seek_result);
  if (seek_result < 0) {
    printf("tail: seek failed\n");
    return 1;
  }

  char buf[256];
  ssize_t n;
  printf("tail_bytes: starting read loop\n");
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    printf("tail_bytes: read %ld bytes\n", n);
    write(1, buf, n);
  }
  printf("tail_bytes: read loop done, n=%ld\n", n);

  return 0;
}

int main(int argc, char *argv[]) {
  int use_bytes = 0;
  int num = DEFAULT_LINES;
  int opt;

  printf("tail: &optind=%p, &optopt=%p, &optarg=%p, &opterr=%p\n", &optind, &optopt, &optarg, &opterr);
  printf("tail: argc=%d\n", argc);
  for (int i = 0; i < argc; i++) {
    printf("tail: argv[%d]=%s\n", i, argv[i]);
  }

  while ((opt = getopt(argc, argv, "c:")) != -1) {
    printf("tail: getopt returned '%c'\n", opt);
    switch (opt) {
      case 'c':
        use_bytes = 1;
        num = atoi(optarg);
        printf("tail: parsed num=%d\n", num);
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

  printf("tail: after getopt, optind=%d\n", optind);

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
