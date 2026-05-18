#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "errno.h"
#include "types.h"

struct utsname {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
};

char kernel_hostname[65] = "sbunix";

DEFINE_SYSCALL1(uname, struct utsname *, user_buf)
{
  if (user_buf == NULL) return -EFAULT;

  struct utsname kbuf;
  strncpy(kbuf.sysname,  "sbunix",   sizeof(kbuf.sysname)  - 1);
  strncpy(kbuf.nodename, kernel_hostname, sizeof(kbuf.nodename) - 1);
  strncpy(kbuf.release,  "0.1.0",    sizeof(kbuf.release)  - 1);
  strncpy(kbuf.version,  "#1",       sizeof(kbuf.version)  - 1);
  strncpy(kbuf.machine,  "riscv64",  sizeof(kbuf.machine)  - 1);

  kbuf.sysname[sizeof(kbuf.sysname)   - 1] = '\0';
  kbuf.nodename[sizeof(kbuf.nodename) - 1] = '\0';
  kbuf.release[sizeof(kbuf.release)   - 1] = '\0';
  kbuf.version[sizeof(kbuf.version)   - 1] = '\0';
  kbuf.machine[sizeof(kbuf.machine)   - 1] = '\0';

  if (copy_to_user(user_buf, &kbuf, sizeof(kbuf)) != 0) return -EFAULT;
  return 0;
}

DEFINE_SYSCALL2(sethostname, const char *, user_name, size_t, len)
{
  if (len == 0 || len >= sizeof(kernel_hostname)) return -EINVAL;
  if (copy_from_user(kernel_hostname, user_name, len) != 0) return -EFAULT;
  kernel_hostname[len] = '\0';
  return 0;
}
