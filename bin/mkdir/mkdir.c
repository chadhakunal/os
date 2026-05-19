#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

static int mkdir_p(const char *path, mode_t mode) {
  char buf[256];
  strncpy(buf, path, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  for (char *p = buf + 1; *p; p++) {
    if (*p != '/') continue;
    *p = '\0';
    if (mkdir(buf, mode) < 0 && errno != EEXIST) return -1;
    *p = '/';
  }
  if (mkdir(buf, mode) < 0 && errno != EEXIST) return -1;
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: mkdir [-p] <dir>...\n");
    return 1;
  }

  int parents = 0;
  int first = 1;
  if (strcmp(argv[1], "-p") == 0) { parents = 1; first = 2; }

  int ret = 0;
  for (int i = first; i < argc; i++) {
    int r;
    if (parents) {
      r = mkdir_p(argv[i], 0755);
    } else {
      r = mkdir(argv[i], 0755);
    }
    if (r < 0) {
      printf("mkdir: failed to create %s\n", argv[i]);
      ret = 1;
    }
  }
  return ret;
}
