#include <arch/riscv64/syscall.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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
  long ret = syscall3(SYS_write, fd, buf, n);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return (ssize_t)ret;
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

int waitid(idtype_t idtype, pid_t id, siginfo_t *info, int options) {
  long ret = syscall4(SYS_waitid, idtype, id, info, options);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
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

pid_t getpgid(pid_t pid) {
  return syscall1(SYS_getpgid, pid);
}

int setpgid(pid_t pid, pid_t pgid) {
  return syscall2(SYS_setpgid, pid, pgid);
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
  return syscall3(SYS_execve, pathname, argv, envp);
}

int execv(const char *pathname, char *const argv[]) {

  return execve(pathname, argv, environ);
}

static char **build_argv(const char *arg, va_list ap) {
  va_list ap2;
  va_copy(ap2, ap);
  int argc = 1;
  while (va_arg(ap2, const char *) != NULL)
    argc++;
  va_end(ap2);

  char **argv = malloc((argc + 1) * sizeof(char *));
  if (!argv)
    return NULL;
  argv[0] = (char *)arg;
  for (int i = 1; i <= argc; i++)
    argv[i] = va_arg(ap, char *);
  return argv;
}

int execl(const char *pathname, const char *arg, ...) {

  va_list ap;
  va_start(ap, arg);
  char **argv = build_argv(arg, ap);
  va_end(ap);
  if (!argv)
    return -1;
  int ret = execve(pathname, argv, environ);
  free(argv);
  return ret;
}

int execle(const char *pathname, const char *arg, ...) {
  va_list ap, ap2;
  va_start(ap, arg);

  /* count args to find where envp sits */
  va_copy(ap2, ap);
  int argc = 1;
  while (va_arg(ap2, const char *) != NULL)
    argc++;
  char **envp = va_arg(ap2, char **);
  va_end(ap2);

  char **argv = build_argv(arg, ap);
  va_end(ap);
  if (!argv)
    return -1;
  int ret = execve(pathname, argv, envp);
  free(argv);
  return ret;
}

int execlp(const char *file, const char *arg, ...) {
  va_list ap;
  va_start(ap, arg);
  char **argv = build_argv(arg, ap);
  va_end(ap);
  if (!argv)
    return -1;
  int ret = execvp(file, argv);
  free(argv);
  return ret;
}

char *getenv(const char *name) {

  if (!environ)
    return NULL;
  size_t nlen = strlen(name);
  for (char **ep = environ; *ep; ep++) {
    if (strncmp(*ep, name, nlen) == 0 && (*ep)[nlen] == '=')
      return *ep + nlen + 1;
  }
  return NULL;
}

int mkstemp(char *tmpl) {
  size_t len = strlen(tmpl);
  if (len < 6 || strcmp(tmpl + len - 6, "XXXXXX") != 0)
    return -1;
  static unsigned seed = 0x12345678;
  for (int tries = 0; tries < 100; tries++) {
    seed = seed * 1664525u + 1013904223u;
    unsigned r = seed;
    for (int i = 0; i < 6; i++) {
      unsigned idx = r % 62;
      r /= 62;
      const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
      tmpl[len - 6 + i] = chars[idx];
    }
    int fd = open(tmpl, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0)
      return fd;
  }
  return -1;
}

int execvp(const char *file, char *const argv[]) {

  if (strchr(file, '/'))
    return execve(file, argv, environ);

  const char *path = getenv("PATH");
  if (!path)
    path = "/bin";

  size_t flen = strlen(file);
  const char *p = path;
  while (1) {
    const char *colon = strchr(p, ':');
    size_t dlen = colon ? (size_t)(colon - p) : strlen(p);
    char *buf = malloc(dlen + 1 + flen + 1);
    if (!buf)
      return -1;
    memcpy(buf, p, dlen);
    buf[dlen] = '/';
    memcpy(buf + dlen + 1, file, flen + 1);
    execve(buf, argv, environ);
    free(buf);
    if (!colon)
      break;
    p = colon + 1;
  }
  return -1;
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

int mkdirat(int dirfd, const char *path, unsigned int mode) {
  return syscall3(SYS_mkdirat, dirfd, path, mode);
}

int unlinkat(int dirfd, const char *path, int flags) {
  return syscall3(SYS_unlinkat, dirfd, path, flags);
}

int linkat(int old_dirfd, const char *oldpath, int new_dirfd, const char *newpath, int flags) {
  return syscall5(SYS_linkat, old_dirfd, oldpath, new_dirfd, newpath, flags);
}

int renameat(int old_dirfd, const char *oldpath, int new_dirfd, const char *newpath) {
  return syscall4(SYS_renameat, old_dirfd, oldpath, new_dirfd, newpath);
}

int rename(const char *oldpath, const char *newpath) {
  return syscall4(SYS_renameat, AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

int link(const char *oldpath, const char *newpath) {
  return syscall5(SYS_linkat, AT_FDCWD, oldpath, AT_FDCWD, newpath, 0);
}

int symlink(const char *target, const char *linkpath) {
  return syscall3(SYS_symlinkat, target, AT_FDCWD, linkpath);
}

int symlinkat(const char *target, int dirfd, const char *linkpath) {
  return syscall3(SYS_symlinkat, target, dirfd, linkpath);
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
  return syscall4(SYS_readlinkat, AT_FDCWD, path, buf, bufsiz);
}

ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz) {
  return syscall4(SYS_readlinkat, dirfd, path, buf, bufsiz);
}

int statfs(const char *path, struct statfs *buf) {
  return syscall2(SYS_statfs, path, buf);
}

int pipe(int pipefd[2]) {
  return syscall1(SYS_pipe, pipefd);
}

int fstat(int fd, struct stat *buf) {
  return syscall2(SYS_fstat, (uint64_t)(int64_t)fd, (uint64_t)buf);
}

int fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
  return syscall4(SYS_fstatat, (uint64_t)(int64_t)dirfd,
                  (uint64_t)path, (uint64_t)buf, (uint64_t)flags);
}

int stat(const char *path, struct stat *buf) {
  return fstatat(AT_FDCWD, path, buf, 0);
}

int lstat(const char *path, struct stat *buf) {
  return fstatat(AT_FDCWD, path, buf, AT_SYMLINK_NOFOLLOW);
}

int chmod(const char *path, unsigned int mode) {
  return syscall2(SYS_chmod, (uint64_t)path, (uint64_t)mode);
}

int truncate(const char *path, off_t length) {
  return syscall2(SYS_truncate, (uint64_t)path, (uint64_t)length);
}

int ftruncate(int fd, off_t length) {
  return syscall2(SYS_ftruncate, (uint64_t)(int64_t)fd, (uint64_t)length);
}

int reboot(int cmd) {
  return (int)syscall1(SYS_reboot, (uint64_t)(int64_t)cmd);
}
