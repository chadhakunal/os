#include <stdio.h>
#include <unistd.h>

int main(void) {
  char buf[4096];
  if (getcwd(buf, sizeof(buf)) == NULL) {
    perror("pwd");
    return 1;
  }
  puts(buf);
  return 0;
}
