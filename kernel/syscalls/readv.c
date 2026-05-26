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

  /* Pipes handle user/kernel copying internally — pass user pointer directly.
   * Regular files need a kernel bounce buffer. */
  int is_pipe = (file->pipe != NULL);

  void *phys_page = NULL;
  char *kbuf = NULL;
  if (!is_pipe) {
    phys_page = get_page(false);
    if (!phys_page)
      return -ENOMEM;
    kbuf = (char *)PHYS_TO_VIRT(phys_page);
  }

  int64_t total = 0;
  for (int i = 0; i < iovcnt; i++) {
    struct kernel_iovec iov;
    if (copy_from_user(&iov, &uiov[i], sizeof(iov)) != 0) {
      if (phys_page) free_page(phys_page);
      return -EFAULT;
    }
    if (iov.iov_len == 0)
      continue;
    if (iov.iov_base == NULL) {
      if (phys_page) free_page(phys_page);
      return -EFAULT;
    }

    int64_t n;
    if (is_pipe) {
      /* pipe_read does its own copy_to_user into iov_base */
      n = vfs_read(file, 0, iov.iov_base, iov.iov_len);
    } else {
      size_t to_read = iov.iov_len > SSIZE_MAX ? SSIZE_MAX : iov.iov_len;
      n = vfs_read(file, file->offset, kbuf, to_read);
      if (n > 0) {
        if (copy_to_user(iov.iov_base, kbuf, (size_t)n) != 0) {
          free_page(phys_page);
          return -EFAULT;
        }
        file->offset += (size_t)n;
      }
    }

    if (n < 0) {
      if (phys_page) free_page(phys_page);
      return total > 0 ? total : n;
    }
    if (n == 0)
      break;

    total += n;
    if (!is_pipe && n < (int64_t)(iov.iov_len > SSIZE_MAX ? SSIZE_MAX : iov.iov_len))
      break;
  }

  if (phys_page) free_page(phys_page);
  return total;
}
