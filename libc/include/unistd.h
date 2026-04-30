#pragma once

#include <types.h>

// Syscall numbers (from kernel)
#define SYS_read            63
#define SYS_write           64
#define SYS_close           57
#define SYS_openat          1024
#define SYS_mmap            222
#define SYS_munmap          215
#define SYS_brk             214
#define SYS_rt_sigaction    134
#define SYS_exit            93
#define SYS_execve          221
#define SYS_wait4           260
#define SYS_getpid          172
#define SYS_kill            129

// RISC-V syscall ABI macros
// Syscall number in a7, args in a0-a5, return value in a0

#define syscall0(n) ({ \
  register long _a0 asm("a0"); \
  register long _syscall_num asm("a7") = (n); \
  asm volatile("ecall" : "=r"(_a0) : "r"(_syscall_num) : "memory"); \
  _a0; \
})

#define syscall1(n, a0) ({ \
  register long _a0 asm("a0") = (long)(a0); \
  register long _syscall_num asm("a7") = (n); \
  asm volatile("ecall" : "+r"(_a0) : "r"(_syscall_num) : "memory"); \
  _a0; \
})

#define syscall2(n, a0, a1) ({ \
  register long _a0 asm("a0") = (long)(a0); \
  register long _a1 asm("a1") = (long)(a1); \
  register long _syscall_num asm("a7") = (n); \
  asm volatile("ecall" : "+r"(_a0) : "r"(_a1), "r"(_syscall_num) : "memory"); \
  _a0; \
})

#define syscall3(n, a0, a1, a2) ({ \
  register long _a0 asm("a0") = (long)(a0); \
  register long _a1 asm("a1") = (long)(a1); \
  register long _a2 asm("a2") = (long)(a2); \
  register long _syscall_num asm("a7") = (n); \
  asm volatile("ecall" : "+r"(_a0) : "r"(_a1), "r"(_a2), "r"(_syscall_num) : "memory"); \
  _a0; \
})

#define syscall4(n, a0, a1, a2, a3) ({ \
  register long _a0 asm("a0") = (long)(a0); \
  register long _a1 asm("a1") = (long)(a1); \
  register long _a2 asm("a2") = (long)(a2); \
  register long _a3 asm("a3") = (long)(a3); \
  register long _syscall_num asm("a7") = (n); \
  asm volatile("ecall" : "+r"(_a0) : "r"(_a1), "r"(_a2), "r"(_a3), "r"(_syscall_num) : "memory"); \
  _a0; \
})

#define syscall5(n, a0, a1, a2, a3, a4) ({ \
  register long _a0 asm("a0") = (long)(a0); \
  register long _a1 asm("a1") = (long)(a1); \
  register long _a2 asm("a2") = (long)(a2); \
  register long _a3 asm("a3") = (long)(a3); \
  register long _a4 asm("a4") = (long)(a4); \
  register long _syscall_num asm("a7") = (n); \
  asm volatile("ecall" : "+r"(_a0) : "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_syscall_num) : "memory"); \
  _a0; \
})

#define syscall6(n, a0, a1, a2, a3, a4, a5) ({ \
  register long _a0 asm("a0") = (long)(a0); \
  register long _a1 asm("a1") = (long)(a1); \
  register long _a2 asm("a2") = (long)(a2); \
  register long _a3 asm("a3") = (long)(a3); \
  register long _a4 asm("a4") = (long)(a4); \
  register long _a5 asm("a5") = (long)(a5); \
  register long _syscall_num asm("a7") = (n); \
  asm volatile("ecall" : "+r"(_a0) : "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5), "r"(_syscall_num) : "memory"); \
  _a0; \
})

// POSIX functions
ssize_t write(int fd, const void *buf, size_t n);
