#define DEBUG 1
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "kernel/memory/page_allocator.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "lib/printk/printk.h"
#include "lib/list.h"
#include "types.h"

#define SSIZE_MAX 4096  // 1 page size

DEFINE_SYSCALL3(write, int, fd, const void *, buf, size_t, count)
{
  debugk("write syscall: fd=%d, buf=%p, count=%llu\n", fd, buf, (uint64_t)count);

  if (fd < 0 || fd >= 32) {
    debugk("write syscall: invalid fd\n");
    return -1;
  }

  if (count > SSIZE_MAX) {
    debugk("write syscall: count clamped from %llu to %d\n", (uint64_t)count, SSIZE_MAX);
    count = SSIZE_MAX;
  }

  struct file_t *file = find_file(&current_task->file_table, fd);

  if (file == NULL) {
    debugk("write syscall: file not found\n");
    return -1;
  }

  debugk("write syscall: allocating page\n");
  // Allocate kernel buffer for safe user data access
  void *phys_page = get_page(false);
  if (!phys_page) {
    debugk("write syscall: page allocation failed\n");
    return -1;
  }

  // Convert physical address to virtual address for kernel access
  char *kernel_buf = (char *)PHYS_TO_VIRT(phys_page);
  debugk("write syscall: kernel_buf=%p, calling copy_from_user\n", kernel_buf);

  // Safely copy from user space to kernel buffer
  if (copy_from_user(kernel_buf, buf, count) != 0) {
    debugk("write syscall: copy_from_user failed\n");
    free_page(phys_page);
    return -1;
  }

  debugk("write syscall: calling vfs_write\n");

  // Determine write offset
  uint64_t write_offset = file->offset;

  // Handle O_APPEND: always write at end of file
  if (file->flags & O_APPEND) {
    // Get file size from vnode
    if (file->vnode) {
      write_offset = file->vnode->size;
      debugk("write syscall: O_APPEND mode, writing at offset %llu (file size)\n", write_offset);
    }
  }

  // Write from kernel buffer
  int64_t bytes_written = vfs_write(file, write_offset, kernel_buf, count);

  debugk("write syscall: vfs_write returned %lld, freeing page\n", bytes_written);
  free_page(phys_page);

  if (bytes_written > 0) {
    // Update offset for next write (even in append mode, offset tracks position)
    file->offset = write_offset + bytes_written;
  }

  debugk("write syscall: returning %lld\n", bytes_written);
  return bytes_written;
}
