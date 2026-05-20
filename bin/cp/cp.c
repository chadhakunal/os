#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

#define BUF_SIZE 4096

static int copy_file(const char *src, const char *dst) {
  int src_fd = open(src, O_RDONLY);
  if (src_fd < 0) {
    printf("cp: cannot open '%s'\n", src);
    return 1;
  }

  int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (dst_fd < 0) {
    printf("cp: cannot create '%s'\n", dst);
    close(src_fd);
    return 1;
  }

  char buf[BUF_SIZE];
  ssize_t n;
  while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
    ssize_t written = 0;
    while (written < n) {
      ssize_t w = write(dst_fd, buf + written, n - written);
      if (w <= 0) {
        printf("cp: write error on '%s'\n", dst);
        close(src_fd);
        close(dst_fd);
        return 1;
      }
      written += w;
    }
  }

  fsync(dst_fd);
  close(src_fd);
  close(dst_fd);
  return 0;
}

static int copy_recursive(const char *src, const char *dst);

static int copy_dir(const char *src, const char *dst) {
  if (mkdir(dst, 0755) < 0) {
    /* If it already exists that's fine, otherwise fail. */
    int fd = open(dst, O_RDONLY);
    if (fd < 0) {
      printf("cp: cannot create directory '%s'\n", dst);
      return 1;
    }
    close(fd);
  }

  int dir_fd = open(src, O_RDONLY);
  if (dir_fd < 0) {
    printf("cp: cannot open directory '%s'\n", src);
    return 1;
  }

  struct dirent entries[64];
  int n = getdents(dir_fd, entries, sizeof(entries));
  close(dir_fd);

  if (n < 0) {
    printf("cp: cannot read directory '%s'\n", src);
    return 1;
  }

  int num = n / sizeof(struct dirent);
  int ret = 0;
  for (int i = 0; i < num; i++) {
    const char *name = entries[i].d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    char src_child[512], dst_child[512];
    snprintf(src_child, sizeof(src_child), "%s/%s", src, name);
    snprintf(dst_child, sizeof(dst_child), "%s/%s", dst, name);

    if (copy_recursive(src_child, dst_child) != 0)
      ret = 1;
  }
  return ret;
}

static int is_dir(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return 0;
  /* Try getdents — succeeds for directories, not for files. */
  struct dirent de;
  int n = getdents(fd, &de, sizeof(de));
  close(fd);
  return (n >= 0);
}

static int copy_recursive(const char *src, const char *dst) {
  if (is_dir(src))
    return copy_dir(src, dst);
  return copy_file(src, dst);
}

int main(int argc, char **argv) {
  int recursive = 0;
  int optind = 1;

  if (argc > 1 && strcmp(argv[1], "-r") == 0) {
    recursive = 1;
    optind = 2;
  }

  if (argc - optind < 2) {
    printf("Usage: cp [-r] <src> <dst>\n");
    return 1;
  }

  const char *src = argv[optind];
  const char *dst = argv[optind + 1];

  if (recursive)
    return copy_recursive(src, dst);
  return copy_file(src, dst);
}
