#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "errno.h"

#define MAX_PATH_COPY 256

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

/* faccessat(dirfd, path, mode, flags) */
DEFINE_SYSCALL4(faccessat,
                int,          dirfd,
                const char *, user_path,
                int,          mode,
                int,          flags)
{
  (void)flags;

  char path[MAX_PATH_COPY];
  copy_string_from_user(path, user_path, MAX_PATH_COPY);

  struct dentry_t *start = task_dirfd_to_dentry(dirfd);
  if (start == (struct dentry_t *)-1)
    return -EBADF;

  struct dentry_t *dentry;
  if (vfs_resolve_path_at(path, start, &dentry, VFS_RESOLVE_FOLLOW_ALL) < 0)
    return -ENOENT;

  if (mode == F_OK)
    return 0;

  struct vnode_t *vnode = dentry->vnode->mounted_vnode
                          ? dentry->vnode->mounted_vnode
                          : dentry->vnode;

  uint32_t m = vnode->permission_mode;
  if ((mode & R_OK) && !(m & 0444))
    return -EACCES;
  if ((mode & W_OK) && !(m & 0222))
    return -EACCES;
  if ((mode & X_OK) && !(m & 0111))
    return -EACCES;

  return 0;
}
