#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "errno.h"

/* fchmod(fd, mode) */
DEFINE_SYSCALL2(fchmod, int, fd, uint32_t, mode)
{
  struct file_t *file = find_file(&current_task->file_table, fd);
  if (file == NULL)
    return -EBADF;

  return vfs_chmod(file->vnode, mode);
}
