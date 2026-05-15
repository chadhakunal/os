#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "types.h"

DEFINE_SYSCALL1(fsync, int, fd)
{
  return vfs_fsync(&current_task->file_table, fd);
}
