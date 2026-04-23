#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "types.h"
#include "arch/riscv64/trap.h"

#pragma once

// RISC-V Linux syscall numbers
// ref: linux/include/uapi/asm-generic/unistd.h
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

void handle_syscall(struct trap_frame *tf);

/* Syscall implementations */
uint64_t sys_openat(struct trap_frame *tf);
uint64_t sys_read(struct trap_frame *tf);
uint64_t sys_write(struct trap_frame *tf);

#endif
