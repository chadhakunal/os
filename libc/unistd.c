#include <unistd.h>
#include <types.h>
#include <stddef.h>

char *getcwd(char *buf, size_t size) {
  if (buf == NULL) {
    return (char *)0;
  }

  long ret = syscall2(SYS_getcwd, buf, size);
  return ret >= 0 ? (char *)ret : (char *)0;
}

int chdir(const char *path) {
  return syscall1(SYS_chdir, path);
}

ssize_t read(int fd, void *buf, size_t n) {
  return syscall3(SYS_read, fd, buf, n);
}

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

int execve(const char *pathname, char *const argv[], char *const envp[]) {
  return syscall3(SYS_execve, pathname, argv, envp);
}
