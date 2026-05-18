#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/user_data_access.h"
#include "errno.h"
#include "types.h"

DEFINE_SYSCALL4(pwrite64, int, fd, const void *, buf, size_t, count, int64_t, offset)
{
  if (offset < 0) return -EINVAL;

  struct file_t *file = find_file(&current_task->file_table, fd);
  if (file == NULL) return -EBADF;
  if ((file->flags & O_ACCMODE) == O_RDONLY) return -EBADF;

  void *phys_page = get_page(false);
  if (!phys_page) return -ENOMEM;
  char *kernel_buf = (char *)PHYS_TO_VIRT(phys_page);

  if (copy_from_user(kernel_buf, buf, count) != 0) {
    free_page(phys_page);
    return -EFAULT;
  }

  int64_t bytes_written = vfs_write(file, (uint64_t)offset, kernel_buf, count);
  free_page(phys_page);
  return bytes_written;
}
