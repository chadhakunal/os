#pragma once

/* Internal: syscall numbers (match kernel) and ecall wrappers. Not for application use. */

#define SYS_getcwd          17
#define SYS_chdir           49
#define SYS_truncate        45
#define SYS_ftruncate       46
#define SYS_lseek           62
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
#define SYS_waitpid         260
#define SYS_getpid          172
#define SYS_getppid         173
#define SYS_kill            129
#define SYS_fork            220
#define SYS_sched_yield     124
#define SYS_ioctl           29
#define SYS_setpgid         154
#define SYS_getdents        61
#define SYS_mkdirat         34
#define SYS_unlinkat        35
#define SYS_dup2            24
#define SYS_fsync           82
#define SYS_nanosleep       101
#define SYS_statfs          43
#define SYS_renameat        38
#define SYS_linkat          37
#define SYS_symlinkat       266
#define SYS_readlinkat      267
#define SYS_pipe            59
#define SYS_fstat           80
#define SYS_fstatat         79
#define SYS_chmod           52
#define SYS_reboot          88
#define SYS_getrlimit       163
#define SYS_setrlimit       164
#define SYS_getpgid          155
#define SYS_rt_sigprocmask   135
#define SYS_rt_sigpending    136
#define SYS_rt_sigsuspend    133
#define SYS_rt_sigtimedwait  137

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
