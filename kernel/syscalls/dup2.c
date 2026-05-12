#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "types.h"

DEFINE_SYSCALL2(dup2, int, oldfd, int, newfd)
{
  return vfs_dup2(&current_task->file_table, oldfd, newfd);
}
