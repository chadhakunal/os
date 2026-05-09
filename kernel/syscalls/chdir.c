#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/user_data_access.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/mode.h"
#include "lib/printk/printk.h"
#include "errno.h"
#include "types.h"

#define MAX_PATH_LEN 256

DEFINE_SYSCALL1(chdir, const char *, user_path)
{
  struct dentry_t *dentry;
  int32_t ret = vfs_resolve_path(user_path, &dentry);

  if (ret < 0) {
    return ret;
  }

  if (dentry == NULL || dentry->vnode == NULL) {
    return -ENOENT;
  }

  if (!IS_DIR(dentry->vnode->permission_mode)) {
    return -ENOTDIR;
  }

  current_task->cwd = dentry;

  return 0;
}
