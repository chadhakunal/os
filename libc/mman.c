#include <arch/riscv64/syscall.h>
#include <sys/mman.h>
#include <errno.h>

void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, long offset) {
  long ret = syscall6(SYS_mmap, addr, len, prot, flags, fd, offset);
  if (ret < 0) {
    errno = (int)(-ret);
    return MAP_FAILED;
  }
  return (void *)ret;
}

int munmap(void *addr, unsigned long len) {
  long ret = syscall2(SYS_munmap, addr, len);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

int mprotect(void *addr, size_t len, int prot) {
  long ret = syscall3(SYS_mprotect, addr, len, prot);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

long brk(unsigned long new_brk) {
  return syscall1(SYS_brk, new_brk);
}

void *sbrk(long increment) {
  long cur = brk(0);
  if (cur < 0)
    return (void *)-1;
  if (increment == 0)
    return (void *)cur;
  long next = brk((unsigned long)(cur + increment));
  if (next < 0 || next == cur)
    return (void *)-1;
  return (void *)cur;
}

int posix_madvise(void *addr, size_t len, int advice) {
  (void)addr;
  (void)len;
  (void)advice;
  return 0;
}

int posix_mem_offset(const void *addr, size_t len, off_t *offset,
                     size_t *contig_len, size_t *alignment) {
  (void)addr;
  (void)len;
  (void)offset;
  (void)contig_len;
  (void)alignment;
  errno = ENOSYS;
  return -1;
}

int posix_typed_mem_open(const char *name, int oflag, int tflag) {
  (void)name;
  (void)oflag;
  (void)tflag;
  errno = ENOSYS;
  return -1;
}

int posix_typed_mem_get_info(int fildes, struct posix_typed_mem_info *info) {
  (void)fildes;
  (void)info;
  errno = ENOSYS;
  return -1;
}
