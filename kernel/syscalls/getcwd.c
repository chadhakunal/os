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
  debugk("getcwd: current_task->cwd=%p, cwd->name=%s, cwd->vnode=%p\n",
         current_task->cwd, current_task->cwd->name, current_task->cwd->vnode);
  int64_t res = vfs_dentry_get_path(current_task->cwd, buf, size);

  if (res < 0) {
    return res;
  }

  return (int64_t)buf;
}
