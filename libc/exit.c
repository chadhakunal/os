#include <arch/riscv64/syscall.h>
#include <stdlib.h>

void exit(int status) {
  syscall1(SYS_exit, status);
  while (1);
}

void _Exit(int status) {
  syscall1(SYS_exit, status);
  while (1);
}
