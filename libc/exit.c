#include <stdlib.h>
#include <unistd.h>

void exit(int status) {
  syscall1(SYS_exit, status);
  // Should never return, but just in case
  while (1);
}
