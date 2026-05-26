#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/task/task.h"
#include "kernel/user_data_access.h"
#include "errno.h"
#include "types.h"

struct kernel_iovec {
  void   *iov_base;
  size_t  iov_len;
};

#define IOV_MAX   1024
#define SSIZE_MAX 4096

DEFINE_SYSCALL3(readv, int, fd, const struct kernel_iovec *, uiov, int, iovcnt)
{
  if (iovcnt <= 0 || iovcnt > IOV_MAX)
    return -EINVAL;

  struct file_t *file = find_file(&current_task->file_table, fd);
  if (file == NULL)
    return -EBADF;
  if ((file->flags & O_ACCMODE) == O_WRONLY)
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

    size_t to_read = iov.iov_len;
    if (to_read > SSIZE_MAX)
      to_read = SSIZE_MAX;

    int64_t n = vfs_read(file, file->offset, kbuf, to_read);
    if (n < 0) {
      free_page(phys_page);
      return total > 0 ? total : n;
    }
    if (n == 0)
      break;

    if (copy_to_user(iov.iov_base, kbuf, (size_t)n) != 0) {
      free_page(phys_page);
      return -EFAULT;
    }

    file->offset += (size_t)n;
    total += n;
    if (n < (int64_t)to_read)
      break;
  }

  free_page(phys_page);
  return total;
}
