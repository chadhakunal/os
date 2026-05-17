#include <arch/riscv64/syscall.h>
#include <stdlib.h>
#include <signal.h>

#define ATEXIT_MAX 32
static void (*atexit_funcs[ATEXIT_MAX])(void);
static int  atexit_count = 0;

static void (*quick_exit_funcs[ATEXIT_MAX])(void);
static int  quick_exit_count = 0;

int atexit(void (*func)(void)) {
  if (atexit_count >= ATEXIT_MAX) return -1;
  atexit_funcs[atexit_count++] = func;
  return 0;
}

int at_quick_exit(void (*func)(void)) {
  if (quick_exit_count >= ATEXIT_MAX) return -1;
  quick_exit_funcs[quick_exit_count++] = func;
  return 0;
}

void quick_exit(int status) {
  for (int i = quick_exit_count - 1; i >= 0; i--)
    quick_exit_funcs[i]();
  _Exit(status);
}

void abort(void) {
  raise(SIGABRT);
  _Exit(128 + SIGABRT);
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

void _exit(int status) {
  syscall1(SYS_exit, status);
  while (1);
}
