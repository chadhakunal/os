#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "errno.h"

#define MAX_PATH_COPY 256

/* fchmodat(dirfd, path, mode, flags) */
DEFINE_SYSCALL4(fchmodat,
                int,          dirfd,
                const char *, user_path,
                uint32_t,     mode,
                int,          flags)
{
  char path[MAX_PATH_COPY];
  copy_string_from_user(path, user_path, MAX_PATH_COPY);

  struct dentry_t *start = task_dirfd_to_dentry(dirfd);
  if (start == (struct dentry_t *)-1)
    return -EBADF;

  uint32_t resolve_flags = VFS_RESOLVE_FOLLOW_ALL;
  if (flags & 0x100) /* AT_SYMLINK_NOFOLLOW */
    resolve_flags = VFS_RESOLVE_NOFOLLOW_FINAL;

  struct dentry_t *dentry;
  if (vfs_resolve_path_at(path, start, &dentry, resolve_flags) < 0)
    return -ENOENT;

  struct vnode_t *vnode = dentry->vnode->mounted_vnode
                          ? dentry->vnode->mounted_vnode
                          : dentry->vnode;

  return vfs_chmod(vnode, mode);
}
