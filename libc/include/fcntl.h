#pragma once

#include <types.h>

// File access modes
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003

// File creation flags
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_CLOEXEC   0x0800

// Special value for dirfd parameter in openat
#define AT_FDCWD    -100

int open(const char *pathname, int flags, ...);
int openat(int dirfd, const char *pathname, int flags, ...);
