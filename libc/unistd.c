#include <unistd.h>
#include <types.h>

ssize_t write(int fd, const void *buf, size_t n) {
  return syscall3(SYS_write, fd, buf, n);
}

pid_t fork(void) {
  return syscall0(SYS_fork);
}

int sched_yield(void) {
  return syscall0(SYS_sched_yield);
}
