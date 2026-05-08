#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

int main(int argc, char *argv[]) {
  char buf[256];

  printf("Testing getcwd...\n");

  char *result = getcwd(buf, sizeof(buf));
  if (result == NULL) {
    printf("getcwd failed\n");
    return 1;
  }

  printf("Current working directory: %s\n", buf);

  if (buf[0] != '/') {
    printf("Error: path doesn't start with /\n");
    return 1;
  }

  printf("getcwd test passed\n");
  return 0;
}
