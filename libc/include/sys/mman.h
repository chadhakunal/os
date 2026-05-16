#pragma once

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    1
#define MAP_PRIVATE   2
#define MAP_FIXED     16
#define MAP_ANONYMOUS 32

void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, long offset);
int   munmap(void *addr, unsigned long len);
void *sbrk(long increment);
long  brk(unsigned long new_brk);
