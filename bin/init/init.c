#include <stdio.h>
#include <unistd.h>
#include <types.h>

int main() {
  pid_t pid = fork();
  while (true) {
    printf("pid = %llu, init.c\n", pid);
  }
  return 0;
}
