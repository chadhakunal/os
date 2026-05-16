#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/pipefs/pipe.h"
#include "kernel/user_data_access.h"
#include "errno.h"

DEFINE_SYSCALL1(pipe, int *, user_pipefd)
{
  struct pipe_t *pipe = pipe_create();
  if (pipe == NULL)
    return -ENOMEM;

  struct file_t *read_file  = file_t_alloc();
  struct file_t *write_file = file_t_alloc();

  read_file->vnode          = NULL;
  read_file->dentry         = NULL;
  read_file->file_ops       = NULL;
  read_file->offset         = 0;
  read_file->refcount       = 1;
  read_file->flags          = 0;
  read_file->pipe           = pipe;
  read_file->pipe_write_end = 0;

  write_file->vnode          = NULL;
  write_file->dentry         = NULL;
  write_file->file_ops       = NULL;
  write_file->offset         = 0;
  write_file->refcount       = 1;
  write_file->flags          = 0;
  write_file->pipe           = pipe;
  write_file->pipe_write_end = 1;

  int read_fd  = alloc_fd(&current_task->file_table, read_file);
  int write_fd = alloc_fd(&current_task->file_table, write_file);

  if (read_fd < 0 || write_fd < 0) {
    if (read_fd >= 0)
      vfs_file_close(&current_task->file_table, read_fd);
    else
      pipe_close(pipe, 0);
    if (write_fd >= 0)
      vfs_file_close(&current_task->file_table, write_fd);
    else
      pipe_close(pipe, 1);
    return -EMFILE;
  }

  int fds[2] = { read_fd, write_fd };
  if (copy_to_user(user_pipefd, fds, sizeof(fds)) != 0)
    return -EFAULT;

  return 0;
}
