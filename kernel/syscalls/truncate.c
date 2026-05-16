#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "errno.h"

#define MAX_PATH_COPY 256

/* truncate(path, length) */
DEFINE_SYSCALL2(truncate, const char *, user_path, int64_t, length)
{
  if (length < 0)
    return -EINVAL;

  char path[MAX_PATH_COPY];
  copy_string_from_user(path, user_path, MAX_PATH_COPY);

  struct dentry_t *dentry;
  if (vfs_resolve_path(path, &dentry) < 0)
    return -ENOENT;

  struct vnode_t *vnode = dentry->vnode->mounted_vnode
                          ? dentry->vnode->mounted_vnode
                          : dentry->vnode;

  return vfs_truncate(vnode, (uint64_t)length);
}
