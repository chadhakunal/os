#pragma once

#include <signal.h>

/* Register save area (s0–s11, sp, ra) — layout must match libc/setjmp.S */
typedef struct {
  unsigned long __regs[14];
  unsigned long __savemask;
  sigset_t __saved_mask;
} jmp_buf[1];

typedef jmp_buf sigjmp_buf;

#if defined(__GNUC__)
#define __setjmp_attr __attribute__((__returns_twice__))
#define __longjmp_attr __attribute__((__noreturn__))
#else
#define __setjmp_attr
#define __longjmp_attr
#endif

int setjmp(jmp_buf env) __setjmp_attr;
void longjmp(jmp_buf env, int val) __longjmp_attr;

int sigsetjmp(sigjmp_buf env, int savemask) __setjmp_attr;
void siglongjmp(sigjmp_buf env, int val) __longjmp_attr; /* asm in setjmp.S */

#undef __setjmp_attr
#undef __longjmp_attr
