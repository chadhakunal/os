#include <sys/uio.h>
#include <errno.h>
#include <arch/riscv64/syscall.h>

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
  long ret = syscall3(SYS_readv, fd, iov, iovcnt);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return (ssize_t)ret;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
  long ret = syscall3(SYS_writev, fd, iov, iovcnt);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return (ssize_t)ret;
}
