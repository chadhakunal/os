#include <arch/riscv64/syscall.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <poll.h>
#include <sys/select.h>

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

int fchdir(int fd) {
  long ret = syscall1(SYS_fchdir, fd);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

ssize_t read(int fd, void *buf, size_t n) {
  long ret = syscall3(SYS_read, fd, buf, n);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return (ssize_t)ret;
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
  long ret = syscall1(SYS_close, fd);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
  long ret = syscall3(SYS_lseek, fd, offset, whence);
  if (ret < 0) {
    errno = (int)(-ret);
    return (off_t)-1;
  }
  return (off_t)ret;
}

pid_t fork(void) {
  return syscall0(SYS_fork);
}

pid_t _Fork(void) {
  return fork();
}

int sched_yield(void) {
  return syscall0(SYS_sched_yield);
}

pid_t waitpid(pid_t pid, int *wstatus, int options) {
  long ret = syscall3(SYS_waitpid, pid, wstatus, options);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return (pid_t)ret;
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

int execveat(int dirfd, const char *pathname, char *const argv[],
             char *const envp[], int flags) {
  return syscall5(SYS_execveat, dirfd, pathname, argv, envp, flags);
}

int fexecve(int fd, char *const argv[], char *const envp[]) {
  return execveat(fd, "", argv, envp, AT_EMPTY_PATH);
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
  long ret = syscall3(SYS_getdents, fd, buf, count);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return (int)ret;
}

long fpathconf(int fd, int name) {
  (void)fd;
  switch (name) {
    case _PC_NAME_MAX:     return NAME_MAX;
    case _PC_FILESIZEBITS: return 64;
    default:
      errno = EINVAL;
      return -1;
  }
}

long pathconf(const char *path, int name) {
  int fd = open(path, O_RDONLY, 0);
  if (fd < 0) return -1;
  long ret = fpathconf(fd, name);
  int saved = errno;
  close(fd);
  errno = saved;
  return ret;
}

int mkdir(const char *path, unsigned int mode) {
  long ret = syscall3(SYS_mkdirat, AT_FDCWD, path, mode);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
}

int unlink(const char *path) {
  long ret = syscall3(SYS_unlinkat, AT_FDCWD, path, 0);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
}

int rmdir(const char *path) {
  long ret = syscall3(SYS_unlinkat, AT_FDCWD, path, AT_REMOVEDIR);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
}

int dup(int oldfd) {
  long ret = syscall3(SYS_fcntl, oldfd, F_DUPFD, 0);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return (int)ret;
}

int dup2(int oldfd, int newfd) {
  long ret = syscall2(SYS_dup2, oldfd, newfd);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return (int)ret;
}

int dup3(int oldfd, int newfd, int flags) {
  long ret = syscall2(SYS_dup2, oldfd, newfd);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  if (flags & O_CLOEXEC) {
    if (fcntl(newfd, F_SETFD, FD_CLOEXEC) < 0)
      return -1;
  }
  return (int)ret;
}

int fsync(int fd) {
  return syscall1(SYS_fsync, fd);
}

int fdatasync(int fd) {
  return fsync(fd);
}

int msync(void *addr, size_t len, int flags) {
  long ret = syscall3(SYS_msync, addr, len, flags);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
}


void sync(void) {
  syscall0(SYS_sync);
}

int uname(struct utsname *buf) {
  if (!buf) { errno = EFAULT; return -1; }
  long ret = syscall1(SYS_uname, buf);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
}

int gethostname(char *name, size_t len) {
  struct utsname u;
  long ret = syscall1(SYS_uname, &u);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  size_t n = strlen(u.nodename);
  if (n >= len) { errno = ENAMETOOLONG; return -1; }
  memcpy(name, u.nodename, n + 1);
  return 0;
}

int sethostname(const char *name, size_t len) {
  long ret = syscall2(SYS_sethostname, name, len);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
  (void)tz;
  if (!tv) return 0;
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) < 0) return -1;
  tv->tv_sec  = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec / 1000;
  return 0;
}

int getentropy(void *buf, size_t len) {
  if (len > GETENTROPY_MAX) { errno = EIO; return -1; }
  long ret = syscall3(SYS_getrandom, buf, len, 0);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
  long ret = syscall4(SYS_pread64, fd, buf, count, offset);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return (ssize_t)ret;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
  long ret = syscall4(SYS_pwrite64, fd, buf, count, offset);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return (ssize_t)ret;
}

int faccessat(int dirfd, const char *path, int mode, int flags) {
  long ret = syscall4(SYS_faccessat, dirfd, path, mode, flags);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
}

int access(const char *path, int mode) {
  return faccessat(AT_FDCWD, path, mode, 0);
}

int isatty(int fd) {
  pid_t pgid;
  if (ioctl(fd, 0x540F /* TIOCGPGRP */, &pgid) == 0) return 1;
  errno = ENOTTY;
  return 0;
}

int pause(void) {
  sigset_t mask;
  sigprocmask(SIG_SETMASK, NULL, &mask);
  return sigsuspend(&mask);
}

int posix_close(int fd, int flags) {
  int ret;
  do { ret = close(fd); } while (ret < 0 && errno == EINTR &&
                                 flags == POSIX_CLOSE_RESTART);
  return ret;
}

size_t confstr(int name, char *buf, size_t len) {
  const char *val = NULL;
  switch (name) {
    case _CS_PATH: val = "/bin"; break;
    default: errno = EINVAL; return 0;
  }
  size_t needed = strlen(val) + 1;
  if (buf && len > 0) {
    size_t copy = needed < len ? needed : len;
    memcpy(buf, val, copy);
    buf[copy - 1] = '\0';
  }
  return needed;
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
  long ret = syscall2(SYS_nanosleep, req, rem);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
}

int mkdirat(int dirfd, const char *path, unsigned int mode) {
  long ret = syscall3(SYS_mkdirat, dirfd, path, mode);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return 0;
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

int mount(const char *source, const char *target, const char *fstype,
          unsigned long flags, const void *data) {
  long ret = syscall5(SYS_mount, source, target, fstype, flags, data);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

int umount(const char *target) {
  long ret = syscall2(SYS_umount2, target, 0);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

int pipe(int pipefd[2]) {
  long ret = syscall2(SYS_pipe2, pipefd, 0);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

int pipe2(int pipefd[2], int flags) {
  long ret = syscall2(SYS_pipe2, pipefd, flags);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

int fstat(int fd, struct stat *buf) {
  long ret = syscall2(SYS_fstat, (uint64_t)(int64_t)fd, (uint64_t)buf);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

int fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
  long ret = syscall4(SYS_fstatat, (uint64_t)(int64_t)dirfd,
                      (uint64_t)path, (uint64_t)buf, (uint64_t)flags);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
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

int fchmod(int fd, mode_t mode) {
  long ret = syscall2(SYS_fchmod, (uint64_t)(int64_t)fd, (uint64_t)mode);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

int fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
  long ret = syscall4(SYS_fchmodat, (uint64_t)(int64_t)dirfd,
                      (uint64_t)path, (uint64_t)mode, (uint64_t)flags);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

mode_t umask(mode_t mask) {
  return (mode_t)syscall1(SYS_umask, (uint64_t)mask);
}

int truncate(const char *path, off_t length) {
  return syscall2(SYS_truncate, (uint64_t)path, (uint64_t)length);
}

int ftruncate(int fd, off_t length) {
  long ret = syscall2(SYS_ftruncate, (uint64_t)(int64_t)fd, (uint64_t)length);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

int reboot(int cmd) {
  return (int)syscall1(SYS_reboot, (uint64_t)(int64_t)cmd);
}

long sysconf(int name) {
  switch (name) {
  case _SC_PAGESIZE:
    return 4096;
  default:
    errno = EINVAL;
    return -1;
  }
}

uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }
pid_t getpgrp(void) { return getpgid(0); }
long gethostid(void) { return 0; }

void swab(const void *from, void *to, ssize_t n) {
  const unsigned char *s = (const unsigned char *)from;
  unsigned char *d = (unsigned char *)to;
  for (ssize_t i = 0; i + 1 < n; i += 2) {
    d[i]     = s[i + 1];
    d[i + 1] = s[i];
  }
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
  long ret = syscall3(SYS_poll, fds, (long)nfds, (long)timeout);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return (int)ret;
}

int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout,
          const sigset_t *sigmask) {
  long ret = syscall4(SYS_ppoll, fds, (long)nfds, timeout, sigmask);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return (int)ret;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout) {
  long ret = syscall5(SYS_select, (long)nfds, readfds, writefds, exceptfds, timeout);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return (int)ret;
}

int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            const struct timespec *timeout, const sigset_t *sigmask) {
  /* Linux pselect6 arg6 is a {sigset_t *, size_t} pair, not a bare pointer. */
  struct { const sigset_t *ptr; unsigned long size; } sm = { sigmask, sizeof(sigset_t) };
  long ret = syscall6(SYS_pselect6, (long)nfds, readfds, writefds, exceptfds, timeout, &sm);
  if (ret < 0) { errno = (int)(-ret); return -1; }
  return (int)ret;
}
