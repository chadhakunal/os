#include <unistd.h>
#include <stdio.h>
#include <stddef.h>

int main(int argc, char *argv[]) {
  char buf[256];

  char *result = getcwd(buf, sizeof(buf));
  if (result == NULL) {
    printf("pwd: error getting current directory\n");
    return 1;
  }

  printf("%s\n", buf);
  return 0;
}
