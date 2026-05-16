#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

int open(const char *pathname, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  return syscall4(SYS_openat, AT_FDCWD, pathname, flags, mode);
}

int openat(int dirfd, const char *pathname, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  return syscall4(SYS_openat, dirfd, pathname, flags, mode);
}
