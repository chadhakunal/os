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

DEFINE_SYSCALL2(statfs, const char *, user_path, struct vfs_statfs *, user_buf)
{
  char path[MAX_PATH_COPY];
  copy_string_from_user(path, user_path, MAX_PATH_COPY);

  struct dentry_t *dentry;
  if (vfs_resolve_path(path, &dentry) < 0) {
    debugk("statfs: path '%s' not found\n", path);
    return -ENOENT;
  }

  struct vnode_t *vnode = dentry->vnode->mounted_vnode
                          ? dentry->vnode->mounted_vnode
                          : dentry->vnode;

  struct vfs_statfs kbuf;
  int64_t ret = vfs_statfs(vnode, &kbuf);
  if (ret < 0)
    return ret;

  copy_to_user(user_buf, &kbuf, sizeof(kbuf));
  return 0;
}
