#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#define BUF_SIZE 4096

/*
 * Try an atomic rename first.  If it fails with EXDEV (cross-device), fall
 * back to a copy-then-delete sequence.  Any other failure is reported and
 * returned as an error.
 */

static int remove_recursive(const char *path);

static int is_dir(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return 0;
  struct dirent de;
  int n = getdents(fd, &de, sizeof(de));
  close(fd);
  return (n >= 0);
}

static int copy_file(const char *src, const char *dst) {
  int src_fd = open(src, O_RDONLY);
  if (src_fd < 0) { fprintf(stderr, "mv: cannot open '%s'\n", src); return 1; }

  int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (dst_fd < 0) {
    fprintf(stderr, "mv: cannot create '%s'\n", dst);
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
        fprintf(stderr, "mv: write error on '%s'\n", dst);
        close(src_fd); close(dst_fd);
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

static int copy_recursive(const char *src, const char *dst) {
  if (!is_dir(src))
    return copy_file(src, dst);

  if (mkdir(dst, 0755) < 0) {
    int fd = open(dst, O_RDONLY);
    if (fd < 0) { fprintf(stderr, "mv: cannot create directory '%s'\n", dst); return 1; }
    close(fd);
  }

  int dir_fd = open(src, O_RDONLY);
  if (dir_fd < 0) { fprintf(stderr, "mv: cannot open '%s'\n", src); return 1; }
  struct dirent entries[64];
  int n = getdents(dir_fd, entries, sizeof(entries));
  close(dir_fd);

  int ret = 0;
  int num = n / sizeof(struct dirent);
  for (int i = 0; i < num; i++) {
    const char *name = entries[i].d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
    char sc[512], dc[512];
    snprintf(sc, sizeof(sc), "%s/%s", src, name);
    snprintf(dc, sizeof(dc), "%s/%s", dst, name);
    if (copy_recursive(sc, dc) != 0) ret = 1;
  }
  return ret;
}

static int remove_file(const char *path) {
  if (unlink(path) < 0) { fprintf(stderr, "mv: cannot remove '%s'\n", path); return 1; }
  return 0;
}

static int remove_dir(const char *path) {
  int dir_fd = open(path, O_RDONLY);
  if (dir_fd < 0) return 1;
  struct dirent entries[64];
  int n = getdents(dir_fd, entries, sizeof(entries));
  close(dir_fd);

  int num = n / sizeof(struct dirent);
  for (int i = 0; i < num; i++) {
    const char *name = entries[i].d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
    char child[512];
    snprintf(child, sizeof(child), "%s/%s", path, name);
    remove_recursive(child);
  }
  if (rmdir(path) < 0) { fprintf(stderr, "mv: cannot remove dir '%s'\n", path); return 1; }
  return 0;
}

static int remove_recursive(const char *path) {
  if (is_dir(path)) return remove_dir(path);
  return remove_file(path);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: mv <src> <dst>\n");
    return 1;
  }

  const char *src = argv[1];
  const char *dst = argv[2];

  /* Try atomic rename first. */
  if (rename(src, dst) == 0)
    return 0;

  /*
   * rename() failed — fall back to copy + delete.
   * This handles cross-filesystem moves (e.g. tarfs -> sbfs).
   */
  if (copy_recursive(src, dst) != 0) {
    fprintf(stderr, "mv: failed to copy '%s' to '%s'\n", src, dst);
    return 1;
  }

  if (remove_recursive(src) != 0) {
    fprintf(stderr, "mv: copied to '%s' but could not remove source '%s'\n", dst, src);
    return 1;
  }

  return 0;
}
