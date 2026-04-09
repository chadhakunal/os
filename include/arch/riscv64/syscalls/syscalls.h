#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "types.h"

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

uint64_t openat(const char *path, uint64_t flags, uint64_t mode);
uint64_t read(uint64_t fd, void *buf, uint64_t size);
uint64_t write(uint64_t fd, void *buff, uint64_t count);

#endif
