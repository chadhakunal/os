#include <arch/riscv64/syscall.h>
#include <stdlib.h>

#define ATEXIT_MAX 32
static void (*atexit_funcs[ATEXIT_MAX])(void);
static int  atexit_count = 0;

int atexit(void (*func)(void)) {
  if (atexit_count >= ATEXIT_MAX) return -1;
  atexit_funcs[atexit_count++] = func;
  return 0;
}

void exit(int status) {
  for (int i = atexit_count - 1; i >= 0; i--)
    atexit_funcs[i]();
  syscall1(SYS_exit, status);
  while (1);
}

void _Exit(int status) {
  syscall1(SYS_exit, status);
  while (1);
}
