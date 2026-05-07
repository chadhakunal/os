#include <fcntl.h>
#include <unistd.h>

int open(const char *pathname, int flags, ...) {
  return syscall4(SYS_openat, AT_FDCWD, pathname, flags, 0);
}

int openat(int dirfd, const char *pathname, int flags, ...) {
  return syscall4(SYS_openat, dirfd, pathname, flags, 0);
}
