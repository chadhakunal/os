#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/task/task.h"
#include "kernel/resource.h"
#include "kernel/user_data_access.h"
#include "errno.h"
#include "types.h"

struct kernel_iovec {
  void   *iov_base;
  size_t  iov_len;
};

#define IOV_MAX   1024
#define SSIZE_MAX 4096

DEFINE_SYSCALL3(writev, int, fd, const struct kernel_iovec *, uiov, int, iovcnt)
{
  if (iovcnt <= 0 || iovcnt > IOV_MAX)
    return -EINVAL;

  struct file_t *file = find_file(&current_task->file_table, fd);
  if (file == NULL)
    return -EBADF;
  if ((file->flags & O_ACCMODE) == O_RDONLY)
    return -EBADF;

  void *phys_page = get_page(false);
  if (!phys_page)
    return -ENOMEM;
  char *kbuf = (char *)PHYS_TO_VIRT(phys_page);

  int64_t total = 0;
  for (int i = 0; i < iovcnt; i++) {
    struct kernel_iovec iov;
    if (copy_from_user(&iov, &uiov[i], sizeof(iov)) != 0) {
      free_page(phys_page);
      return -EFAULT;
    }
    if (iov.iov_len == 0)
      continue;
    if (iov.iov_base == NULL) {
      free_page(phys_page);
      return -EFAULT;
    }

    size_t to_write = iov.iov_len;
    if (to_write > SSIZE_MAX)
      to_write = SSIZE_MAX;

    if (copy_from_user(kbuf, iov.iov_base, to_write) != 0) {
      free_page(phys_page);
      return -EFAULT;
    }

    uint64_t write_offset = file->offset;
    if (file->flags & O_APPEND)
      write_offset = file->vnode ? file->vnode->size : file->offset;

    if (file->vnode) {
      uint64_t new_size = write_offset + to_write;
      if (file->vnode->size > new_size)
        new_size = file->vnode->size;
      int fsize_ret = rlimit_check_fsize(current_task, new_size);
      if (fsize_ret < 0) {
        free_page(phys_page);
        return total > 0 ? total : fsize_ret;
      }
    }

    int64_t n = vfs_write(file, write_offset, kbuf, to_write);
    if (n < 0) {
      free_page(phys_page);
      return total > 0 ? total : n;
    }

    if (file->pipe == NULL)
      file->offset = write_offset + (size_t)n;
    total += n;
  }

  free_page(phys_page);
  return total;
}
