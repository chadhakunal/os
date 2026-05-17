#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "types.h"
#include "arch/riscv64/trap.h"

#pragma once

// RISC-V Linux syscall numbers
// ref: linux/include/uapi/asm-generic/unistd.h
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
#define SYS_mprotect        226
#define SYS_brk             214
#define SYS_rt_sigaction    134
#define SYS_rt_sigreturn    139
#define SYS_exit            93
#define SYS_execve          221
#define SYS_wait4           260
#define SYS_waitpid         260  // Same as wait4 on Linux
#define SYS_getpid          172
#define SYS_getppid         173
#define SYS_kill            129
#define SYS_fork            220
#define SYS_sched_yield     124
#define SYS_ioctl           29
#define SYS_fcntl           25
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
#define SYS_pipe2           59
#define SYS_pipe            SYS_pipe2
#define SYS_fchdir          50
#define SYS_execveat        281
#define SYS_fstat           80
#define SYS_fstatat         79
#define SYS_chmod           52
#define SYS_fchmodat        53
#define SYS_fchmod          91
#define SYS_umask           166
#define SYS_reboot          88
#define SYS_getrlimit       163
#define SYS_setrlimit       164
#define SYS_getpgid          155
#define SYS_waitid           95
#define SYS_rt_sigprocmask   135
#define SYS_rt_sigpending    136
#define SYS_rt_sigsuspend    133
#define SYS_rt_sigtimedwait  137
#define SYS_poll             73
#define SYS_ppoll            89

void handle_syscall(struct trap_frame *tf);

/* Syscall implementations */
int64_t sys_getcwd(struct trap_frame *tf);
int64_t sys_chdir(struct trap_frame *tf);
int64_t sys_openat(struct trap_frame *tf);
int64_t sys_read(struct trap_frame *tf);
int64_t sys_write(struct trap_frame *tf);
int64_t sys_close(struct trap_frame *tf);
int64_t sys_truncate(struct trap_frame *tf);
int64_t sys_ftruncate(struct trap_frame *tf);
int64_t sys_lseek(struct trap_frame *tf);
int64_t sys_fork(struct trap_frame *tf);
int64_t sys_sched_yield(struct trap_frame *tf);
int64_t sys_waitpid(struct trap_frame *tf);
int64_t sys_exit(struct trap_frame *tf);
int64_t sys_execve(struct trap_frame *tf);
int64_t sys_rt_sigaction(struct trap_frame *tf);
int64_t sys_rt_sigreturn(struct trap_frame *tf);
int64_t sys_kill(struct trap_frame *tf);
int64_t sys_ioctl(struct trap_frame *tf);
int64_t sys_fcntl(struct trap_frame *tf);
int64_t sys_getpid(struct trap_frame *tf);
int64_t sys_getppid(struct trap_frame *tf);
int64_t sys_setpgid(struct trap_frame *tf);
int64_t sys_getdents(struct trap_frame *tf);
int64_t sys_mkdirat(struct trap_frame *tf);
int64_t sys_unlinkat(struct trap_frame *tf);
int64_t sys_dup2(struct trap_frame *tf);
int64_t sys_fsync(struct trap_frame *tf);
int64_t sys_nanosleep(struct trap_frame *tf);
int64_t sys_mmap(struct trap_frame *tf);
int64_t sys_munmap(struct trap_frame *tf);
int64_t sys_mprotect(struct trap_frame *tf);
int64_t sys_brk(struct trap_frame *tf);
int64_t sys_statfs(struct trap_frame *tf);
int64_t sys_renameat(struct trap_frame *tf);
int64_t sys_linkat(struct trap_frame *tf);
int64_t sys_symlinkat(struct trap_frame *tf);
int64_t sys_readlinkat(struct trap_frame *tf);
int64_t sys_pipe2(struct trap_frame *tf);
int64_t sys_fchdir(struct trap_frame *tf);
int64_t sys_execveat(struct trap_frame *tf);
int64_t sys_fstat(struct trap_frame *tf);
int64_t sys_fstatat(struct trap_frame *tf);
int64_t sys_chmod(struct trap_frame *tf);
int64_t sys_fchmod(struct trap_frame *tf);
int64_t sys_fchmodat(struct trap_frame *tf);
int64_t sys_umask(struct trap_frame *tf);
int64_t sys_reboot(struct trap_frame *tf);
int64_t sys_getrlimit(struct trap_frame *tf);
int64_t sys_setrlimit(struct trap_frame *tf);
int64_t sys_getpgid(struct trap_frame *tf);
int64_t sys_waitid(struct trap_frame *tf);
int64_t sys_rt_sigprocmask(struct trap_frame *tf);
int64_t sys_rt_sigpending(struct trap_frame *tf);
int64_t sys_rt_sigsuspend(struct trap_frame *tf);
int64_t sys_rt_sigtimedwait(struct trap_frame *tf);
int64_t sys_poll(struct trap_frame *tf);
int64_t sys_ppoll(struct trap_frame *tf);

#endif
