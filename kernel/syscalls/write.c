#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "lib/list.h"
#include "types.h"

DEFINE_SYSCALL3(write, int, fd, const void *, buf, size_t, count)
{
  // Validate fd range
  if (fd < 0 || fd >= 32) {
    return -1;  // EBADF
  }

  // Look up file descriptor in current task's file table
  struct file_t *file = NULL;
  list_for_each(&current_task->file_table.files_list, pos) {
    struct files_list_t *files_list = container_of(pos, struct files_list_t, files_list);

    // Check if this fd is marked as used in the bitmap
    if (files_list->used_file_bitmap & (1 << fd)) {
      file = files_list->files[fd];
      break;
    }
  }

  if (file == NULL) {
    return -1;  // EBADF - file descriptor not open
  }

  // Perform the write via VFS
  int64_t bytes_written = vfs_write(file, file->offset, (void *)buf, count);

  if (bytes_written > 0) {
    // Update file offset
    file->offset += bytes_written;
  }

  return bytes_written;
}
