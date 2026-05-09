#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "errno.h"
#include "types.h"

// lseek whence values
#define SEEK_SET 0  // Seek from beginning of file
#define SEEK_CUR 1  // Seek from current position
#define SEEK_END 2  // Seek from end of file

DEFINE_SYSCALL3(lseek, int, fd, int64_t, offset, int, whence)
{
  return vfs_file_lseek(&current_task->file_table, fd, offset, whence);
}
