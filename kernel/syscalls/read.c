#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "types.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"

DEFINE_SYSCALL3(read, int, fd, const void *, buf, size_t, count) {
  if (fd < 0 || fd >= 32) {
    return -1;
  }

  struct file_t *file = find_file(&current_task->file_table, fd);

  if (file == NULL) {
    return -1;
  }

  int64_t bytes_read = vfs_read(file, file->offset, (void *)buf, count);

  if (bytes_read > 0) {
    file->offset += bytes_read ;
  }

  return bytes_read;
}
