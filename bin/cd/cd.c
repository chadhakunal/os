#include <unistd.h>
#include <stdio.h>
#include <stddef.h>

int main(int argc, char *argv[]) {
  const char *path;

  if (argc < 2) {
    path = "/";
  } else {
    path = argv[1];
  }

  if (chdir(path) < 0) {
    printf("cd: cannot change directory to '%s'\n", path);
    return 1;
  }

  return 0;
}
