#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/mode.h"
#include "errno.h"

DEFINE_SYSCALL1(fchdir, int, fd)
{
  struct file_t *file = find_file(&current_task->file_table, fd);
  if (file == NULL)
    return -EBADF;

  if (file->dentry == NULL)
    return -ENOTDIR;

  struct vnode_t *vnode = file->dentry->vnode;
  if (vnode == NULL)
    return -ENOENT;

  if (vnode->mounted_vnode)
    vnode = vnode->mounted_vnode;

  if (!IS_DIR(vnode->permission_mode))
    return -ENOTDIR;

  current_task->cwd = file->dentry;
  return 0;
}
