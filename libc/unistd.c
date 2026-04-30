#include <unistd.h>
#include <types.h>

ssize_t write(int fd, const void *buf, size_t n) {
  return syscall3(SYS_write, fd, buf, n);
}
