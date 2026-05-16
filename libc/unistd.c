#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <types.h>
#include <stddef.h>
#include <time.h>

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

int close(int fd) {
  return syscall1(SYS_close, fd);
}

off_t lseek(int fd, off_t offset, int whence) {
  return syscall3(SYS_lseek, fd, offset, whence);
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

int ioctl(int fd, unsigned long request, void *arg) {
  return syscall3(SYS_ioctl, fd, request, arg);
}

pid_t tcgetpgrp(int fd) {
  pid_t pgid;
  if (ioctl(fd, 0x540F, &pgid) < 0) {
    return -1;
  }
  return pgid;
}

int tcsetpgrp(int fd, pid_t pgid) {
  return ioctl(fd, 0x5410, &pgid);
}

pid_t getpid(void) {
  return syscall0(SYS_getpid);
}

pid_t getppid(void) {
  return syscall0(SYS_getppid);
}

int setpgid(pid_t pid, pid_t pgid) {
  return syscall2(SYS_setpgid, pid, pgid);
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
  return syscall3(SYS_execve, pathname, argv, envp);
}

int getdents(int fd, struct dirent *buf, unsigned int count) {
  return syscall3(SYS_getdents, fd, buf, count);
}

int mkdir(const char *path, unsigned int mode) {
  return syscall3(SYS_mkdirat, AT_FDCWD, path, mode);
}

int unlink(const char *path) {
  return syscall3(SYS_unlinkat, AT_FDCWD, path, 0);
}

int rmdir(const char *path) {
  return syscall3(SYS_unlinkat, AT_FDCWD, path, AT_REMOVEDIR);
}

int dup2(int oldfd, int newfd) {
  return syscall2(SYS_dup2, oldfd, newfd);
}

int fsync(int fd) {
  return syscall1(SYS_fsync, fd);
}

unsigned int sleep(unsigned int seconds) {
  struct timespec req = { .tv_sec = seconds, .tv_nsec = 0 };
  struct timespec rem = { 0, 0 };
  if (nanosleep(&req, &rem) == 0)
    return 0;
  return (unsigned int)rem.tv_sec;
}

int usleep(unsigned long usec) {
  struct timespec req = {
    .tv_sec  = (long)(usec / 1000000),
    .tv_nsec = (long)(usec % 1000000) * 1000,
  };
  return nanosleep(&req, (struct timespec *)0);
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
  return syscall2(SYS_nanosleep, req, rem);
}

int rename(const char *oldpath, const char *newpath) {
  return syscall4(SYS_renameat, AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

int statfs(const char *path, struct statfs *buf) {
  return syscall2(SYS_statfs, path, buf);
}
