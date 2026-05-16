#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "errno.h"

#define MAX_PATH_COPY 256

/* fstatat(dirfd, path, statbuf, flags) */
DEFINE_SYSCALL4(fstatat,
                int,              dirfd,
                const char *,     user_path,
                struct vfs_stat *, user_buf,
                int,              flags)
{
  (void)flags;

  char path[MAX_PATH_COPY];
  copy_string_from_user(path, user_path, MAX_PATH_COPY);

  struct dentry_t *start = task_dirfd_to_dentry(dirfd);
  if (start == (struct dentry_t *)-1) return -EBADF;

  struct dentry_t *dentry;
  if (vfs_resolve_path_at(path, start, &dentry) < 0) {
    return -ENOENT;
  }

  struct vnode_t *vnode = dentry->vnode->mounted_vnode
                          ? dentry->vnode->mounted_vnode
                          : dentry->vnode;

  struct vfs_stat kbuf;
  int64_t ret = vfs_stat(vnode, &kbuf);
  if (ret < 0) return ret;

  copy_to_user(user_buf, &kbuf, sizeof(kbuf));
  return 0;
}
