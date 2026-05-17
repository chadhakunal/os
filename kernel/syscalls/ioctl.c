#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "types.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "errno.h"

DEFINE_SYSCALL3(ioctl, int, fd, unsigned long, request, void *, arg) {
  struct file_t *file = find_file(&current_task->file_table, fd);

  if (file == NULL)
    return -EBADF;

  if (file->file_ops == NULL || file->file_ops->ioctl == NULL) {
    return -ENOTTY;
  }

  return file->file_ops->ioctl(file, request, arg);
}
