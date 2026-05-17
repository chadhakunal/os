#include <arch/riscv64/syscall.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>

int open(const char *pathname, int flags, ...) {
  mode_t mode = 0;
  long ret;

  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  ret = syscall4(SYS_openat, AT_FDCWD, pathname, flags, mode);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return (int)ret;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
  mode_t mode = 0;
  long ret;

  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  ret = syscall4(SYS_openat, dirfd, pathname, flags, mode);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return (int)ret;
}
