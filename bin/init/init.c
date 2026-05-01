#include <stdio.h>
#include <unistd.h>
#include <types.h>

int main() {
  pid_t pid = fork();
  printf("pid = %llu, init.c\n", pid);
  return 0;
}
