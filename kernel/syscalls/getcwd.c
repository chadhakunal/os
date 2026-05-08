#define DEBUG 1
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "errno.h"
#include "types.h"
#include "lib/printk/printk.h"

DEFINE_SYSCALL2(getcwd, char *, buf, size_t, size)
{
  if (buf == NULL || size == 0) {
    return -EINVAL;
  }
  debugk("cwd: %s\n", current_task->cwd);
  int64_t res = vfs_dentry_get_path(current_task->cwd, buf, size);

  if (res < 0) {
    return res;
  }

  return (int64_t)buf;
}
