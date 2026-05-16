#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#define DEBUG 0
#include "lib/printk/printk.h"
#include "errno.h"

#define MAX_PATH_COPY 256

/* chmod(path, mode) */
DEFINE_SYSCALL2(chmod, const char *, user_path, uint32_t, mode)
{
  char path[MAX_PATH_COPY];
  copy_string_from_user(path, user_path, MAX_PATH_COPY);

  struct dentry_t *dentry;
  if (vfs_resolve_path(path, &dentry) < 0) {
    debugk("chmod: path '%s' not found\n", path);
    return -ENOENT;
  }

  struct vnode_t *vnode = dentry->vnode->mounted_vnode
                          ? dentry->vnode->mounted_vnode
                          : dentry->vnode;

  int64_t ret = vfs_chmod(vnode, mode);
  if (ret < 0)
    debugk("chmod: failed on '%s': %lld\n", path, ret);
  return ret;
}
