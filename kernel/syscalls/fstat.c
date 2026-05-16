#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "errno.h"

/* fstat(fd, statbuf) */
DEFINE_SYSCALL2(fstat, int, fd, struct vfs_stat *, user_buf)
{
  struct file_t *file = find_file(&current_task->file_table, fd);
  if (file == NULL)
    return -EBADF;

  struct vfs_stat kbuf;
  int64_t ret = vfs_stat(file->vnode, &kbuf);
  if (ret < 0)
    return ret;

  copy_to_user(user_buf, &kbuf, sizeof(kbuf));
  return 0;
}
