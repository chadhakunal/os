#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "errno.h"

DEFINE_SYSCALL3(fcntl, int, fd, int, cmd, uint64_t, arg) {
  struct file_t *file;

  switch (cmd) {
    case F_DUPFD:
      return vfs_fcntl_dup(&current_task->file_table, fd, (int)arg);

    case F_GETFD:
      return vfs_file_get_fd_flags(&current_task->file_table, fd);

    case F_SETFD:
      return vfs_file_set_fd_flags(&current_task->file_table, fd, (int)arg);

    case F_GETFL:
      file = find_file(&current_task->file_table, fd);
      if (file == NULL)
        return -EBADF;
      return file->flags;

    case F_SETFL:
      file = find_file(&current_task->file_table, fd);
      if (file == NULL)
        return -EBADF;
      /* Only update the settable flags: O_APPEND, O_NONBLOCK */
      file->flags = (file->flags & ~(O_APPEND | O_NONBLOCK))
                  | ((int)arg   &  (O_APPEND | O_NONBLOCK));
      return 0;

    default:
      return -EINVAL;
  }
}
