#include <sys/mman.h>
#include <unistd.h>

void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, long offset) {
  long ret = syscall6(SYS_mmap, addr, len, prot, flags, fd, offset);
  if (ret < 0) return (void *)-1;
  return (void *)ret;
}

int munmap(void *addr, unsigned long len) {
  return (int)syscall2(SYS_munmap, addr, len);
}

long brk(unsigned long new_brk) {
  return syscall1(SYS_brk, new_brk);
}

void *sbrk(long increment) {
  long cur = brk(0);
  if (cur < 0) return (void *)-1;
  if (increment == 0) return (void *)cur;
  long next = brk((unsigned long)(cur + increment));
  if (next < 0 || next == cur) return (void *)-1;
  return (void *)cur;
}
