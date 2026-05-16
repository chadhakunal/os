#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/resource.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "errno.h"

/* ftruncate(fd, length) */
DEFINE_SYSCALL2(ftruncate, int, fd, int64_t, length)
{
  if (length < 0)
    return -EINVAL;

  struct file_t *file = find_file(&current_task->file_table, fd);
  if (file == NULL)
    return -EBADF;

  int fsize_ret = rlimit_check_fsize(current_task, (uint64_t)length);
  if (fsize_ret < 0)
    return fsize_ret;

  int64_t ret = vfs_truncate(file->vnode, (uint64_t)length);
  if (ret < 0)
    return ret;

  if (file->offset > (size_t)length)
    file->offset = (size_t)length;
  return 0;
}
