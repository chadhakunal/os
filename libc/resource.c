#include <sys/resource.h>
#include <arch/riscv64/syscall.h>
#include <errno.h>

int getrlimit(int resource, struct rlimit *rlim) {
  long ret = syscall2(SYS_getrlimit, resource, rlim);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}

int setrlimit(int resource, const struct rlimit *rlim) {
  long ret = syscall2(SYS_setrlimit, resource, rlim);
  if (ret < 0) {
    errno = (int)(-ret);
    return -1;
  }
  return 0;
}
