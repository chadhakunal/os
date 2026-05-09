#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "errno.h"
#include "types.h"

DEFINE_SYSCALL1(close, int, fd)
{
  return vfs_file_close(&current_task->file_table, fd);
}
