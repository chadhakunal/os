#pragma once

#include <stddef.h>
#include <sys/types.h>

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20

#define MAP_FAILED ((void *)-1)

#define POSIX_MADV_NORMAL     0
#define POSIX_MADV_RANDOM     1
#define POSIX_MADV_SEQUENTIAL 2
#define POSIX_MADV_WILLNEED   3
#define POSIX_MADV_DONTNEED   4

struct posix_typed_mem_info {
  size_t posix_tmi_length;
  size_t posix_tmi_least_alloc;
  size_t posix_tmi_most_alloc;
};

void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, long offset);
int   munmap(void *addr, unsigned long len);
int   mprotect(void *addr, size_t len, int prot);
int   posix_madvise(void *addr, size_t len, int advice);
int   posix_mem_offset(const void *addr, size_t len, off_t *offset,
                       size_t *contig_len, size_t *alignment);
int   posix_typed_mem_open(const char *name, int oflag, int tflag);
int   posix_typed_mem_get_info(int fildes, struct posix_typed_mem_info *info);

/* msync flags */
#define MS_ASYNC      1
#define MS_SYNC       4
#define MS_INVALIDATE 2

int msync(void *addr, size_t len, int flags);

void *sbrk(long increment);
long  brk(unsigned long new_brk);
