#pragma once

#include <sys/types.h>

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003

#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_CLOEXEC   0x0800
#define O_DIRECTORY 0x10000

#define AT_FDCWD            -100
#define AT_REMOVEDIR        0x200
#define AT_SYMLINK_NOFOLLOW 0x100

#define FD_CLOEXEC 1
#define F_GETFD    1
#define F_SETFD    2
#define F_GETFL    3
#define F_SETFL    4

#define POSIX_FADV_NORMAL     0
#define POSIX_FADV_SEQUENTIAL 2

int open(const char *pathname, int flags, ...);
int openat(int dirfd, const char *pathname, int flags, ...);
int creat(const char *pathname, mode_t mode);
int fcntl(int fd, int cmd, ...);

int posix_fadvise(int fd, off_t offset, off_t len, int advice);
int posix_fallocate(int fd, off_t offset, off_t len);
