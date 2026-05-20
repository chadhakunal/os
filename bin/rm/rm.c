#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

static int is_dir(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return 0;
  struct dirent de;
  int n = getdents(fd, &de, 1);
  close(fd);
  return (n > 0);
}

static int remove_recursive(const char *path);

static int remove_dir(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) { fprintf(stderr, "rm: cannot open '%s'\n", path); return 1; }
  int ret = 0;
  struct dirent entries[64];
  int n;
  while ((n = getdents(fd, entries, 64)) > 0) {
    for (int i = 0; i < n; i++) {
      const char *name = entries[i].d_name;
      if (name[0] == '\0') continue;
      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
      char child[512];
      int pl = strlen(path), nl = strlen(name);
      if (pl + nl + 2 > (int)sizeof(child)) continue;
      memcpy(child, path, pl);
      child[pl] = '/';
      memcpy(child + pl + 1, name, nl + 1);
      if (remove_recursive(child) != 0) ret = 1;
    }
  }
  close(fd);
  if (rmdir(path) < 0) { fprintf(stderr, "rm: cannot remove dir '%s'\n", path); return 1; }
  return ret;
}

static int remove_recursive(const char *path) {
  if (is_dir(path)) return remove_dir(path);
  if (unlink(path) < 0) { fprintf(stderr, "rm: cannot remove '%s'\n", path); return 1; }
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: rm [-rf] <file>...\n");
    return 1;
  }
  int recursive = 0;
  int force = 0;
  int first = 1;

  /* Parse flags: handle -r, -f, -rf, -fr, -r -f, -f -r, etc. */
  while (first < argc && argv[first][0] == '-') {
    const char *p = argv[first] + 1;
    if (*p == '\0') break; /* bare '-' treated as filename */
    for (; *p; p++) {
      if (*p == 'r') recursive = 1;
      else if (*p == 'f') force = 1;
      else {
        fprintf(stderr, "rm: unknown option '-%c'\n", *p);
        return 1;
      }
    }
    first++;
  }

  if (first >= argc) {
    if (force) return 0; /* rm -f with no args is OK */
    fprintf(stderr, "Usage: rm [-rf] <file>...\n");
    return 1;
  }
  int ret = 0;
  for (int i = first; i < argc; i++) {
    if (recursive) {
      /* Check existence first; with -f, missing paths are not an error */
      int fd = open(argv[i], O_RDONLY);
      if (fd < 0) {
        if (!force) {
          fprintf(stderr, "rm: cannot remove '%s'\n", argv[i]);
          ret = 1;
        }
        continue;
      }
      close(fd);
      if (remove_recursive(argv[i]) != 0) ret = 1;
    } else {
      if (unlink(argv[i]) < 0) {
        if (!force) {
          fprintf(stderr, "rm: cannot remove '%s'\n", argv[i]);
          ret = 1;
        }
      }
    }
  }
  return ret;
}
