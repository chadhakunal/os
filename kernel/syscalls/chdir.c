#define DEBUG 1
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
  debugk("chdir: path=%s\n", user_path);

  struct dentry_t *dentry;
  int32_t ret = vfs_resolve_path(user_path, &dentry);

  if (ret < 0) {
    debugk("chdir: vfs_resolve_path failed with %d\n", ret);
    return ret;
  }

  debugk("chdir: resolved to dentry=%p\n", dentry);

  if (dentry == NULL || dentry->vnode == NULL) {
    debugk("chdir: dentry or vnode is NULL\n");
    return -ENOENT;
  }

  debugk("chdir: vnode=%p, permission_mode=0x%x\n", dentry->vnode, dentry->vnode->permission_mode);

  if (!IS_DIR(dentry->vnode->permission_mode)) {
    debugk("chdir: path is not a directory\n");
    return -ENOTDIR;
  }

  current_task->cwd = dentry;
  debugk("chdir: success, new cwd=%p name=%s\n", current_task->cwd->name);

  return 0;
}
