#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <arch/riscv64/syscall.h>

int creat(const char *pathname, mode_t mode) {
  return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int fcntl(int fd, int cmd, ...) {
  unsigned long arg = 0;
  va_list ap;

  va_start(ap, cmd);
  arg = va_arg(ap, unsigned long);
  va_end(ap);

  long ret = syscall3(SYS_fcntl, fd, cmd, arg);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return (int)ret;
}

int posix_fadvise(int fd, off_t offset, off_t len, int advice) {
  (void)fd;
  (void)offset;
  (void)len;
  (void)advice;
  return 0;
}

int posix_fallocate(int fd, off_t offset, off_t len) {
  if (len < 0) {
    errno = EINVAL;
    return -1;
  }
  if (len == 0)
    return 0;

  off_t end = offset + len;
  if (end < offset) {
    errno = EFBIG;
    return -1;
  }

  if (ftruncate(fd, end) < 0)
    return -1;
  return 0;
}
