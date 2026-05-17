#pragma once

extern char **environ;

void _exit(int status);

#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdint.h>

#define RB_POWER_OFF  0
#define RB_AUTOBOOT   1

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define TCSRAW   0x5411
#define TCSCANON 0x5412

#define _PC_NAME_MAX 4

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;

struct statfs {
  long f_type;
  long f_bsize;
  long f_blocks;
  long f_bfree;
  long f_bavail;
  long f_files;
  long f_ffree;
  long f_fsid[2];
  long f_namelen;
  long f_frsize;
  long f_flags;
  long f_spare[4];
};

int getopt(int argc, char *const argv[], const char *optstring);

char *getcwd(char *buf, size_t size);
int chdir(const char *path);
ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
pid_t fork(void);
int sched_yield(void);
int kill(pid_t pid, int sig);
int ioctl(int fd, unsigned long request, void *arg);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgid);
pid_t getpid(void);
pid_t getppid(void);
int setpgid(pid_t pid, pid_t pgid);
pid_t getpgid(pid_t pid);
int execve(const char *pathname, char *const argv[], char *const envp[]);
int execv(const char *pathname, char *const argv[]);
int execl(const char *pathname, const char *arg, ...);
int execle(const char *pathname, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
int execvp(const char *file, char *const argv[]);
int mkdir(const char *path, mode_t mode);
int unlink(const char *path);
int rmdir(const char *path);
int dup2(int oldfd, int newfd);
int fsync(int fd);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned long usec);
int mkdirat(int dirfd, const char *path, mode_t mode);
int unlinkat(int dirfd, const char *path, int flags);
int linkat(int old_dirfd, const char *oldpath, int new_dirfd,
           const char *newpath, int flags);
int renameat(int old_dirfd, const char *oldpath, int new_dirfd,
             const char *newpath);
int rename(const char *oldpath, const char *newpath);
int link(const char *oldpath, const char *newpath);
int symlink(const char *target, const char *linkpath);
int symlinkat(const char *target, int dirfd, const char *linkpath);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);
ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz);
int pipe(int pipefd[2]);
int truncate(const char *path, off_t length);
int ftruncate(int fd, off_t length);
int reboot(int cmd);
int statfs(const char *path, struct statfs *buf);
long fpathconf(int fd, int name);
