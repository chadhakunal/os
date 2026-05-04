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

pid_t waitpid(pid_t pid, int *wstatus, int options) {
  return syscall3(SYS_waitpid, pid, wstatus, options);
}

pid_t wait(int *wstatus) {
  return waitpid(-1, wstatus, 0);
}
