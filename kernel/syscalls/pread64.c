#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "errno.h"
#include "types.h"

DEFINE_SYSCALL4(pread64, int, fd, void *, buf, size_t, count, int64_t, offset)
{
  if (offset < 0) return -EINVAL;

  struct file_t *file = find_file(&current_task->file_table, fd);
  if (file == NULL) return -EBADF;
  if ((file->flags & O_ACCMODE) == O_WRONLY) return -EBADF;

  return vfs_read(file, (uint64_t)offset, buf, count);
}
