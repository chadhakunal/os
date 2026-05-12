#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "kernel/memory/page_allocator.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "lib/list.h"
#include "types.h"

#define SSIZE_MAX 4096  // 1 page size

DEFINE_SYSCALL3(write, int, fd, const void *, buf, size_t, count)
{
  if (fd < 0 || fd >= 32) {
    return -1;
  }

  if (count > SSIZE_MAX) {
    count = SSIZE_MAX;
  }

  struct file_t *file = find_file(&current_task->file_table, fd);

  if (file == NULL) {
    return -1;
  }

  // Allocate kernel buffer for safe user data access
  void *phys_page = get_page(false);
  if (!phys_page) {
    return -1;
  }

  // Convert physical address to virtual address for kernel access
  char *kernel_buf = (char *)PHYS_TO_VIRT(phys_page);

  // Safely copy from user space to kernel buffer
  if (copy_from_user(kernel_buf, buf, count) != 0) {
    free_page(phys_page);
    return -1;
  }

  // Write from kernel buffer
  int64_t bytes_written = vfs_write(file, file->offset, kernel_buf, count);

  free_page(phys_page);

  if (bytes_written > 0) {
    file->offset += bytes_written;
  }

  return bytes_written;
}
