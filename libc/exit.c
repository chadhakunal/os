#include <arch/riscv64/syscall.h>
#include <stdlib.h>

void exit(int status) {
  syscall1(SYS_exit, status);
  // Should never return, but just in case
  while (1);
}
