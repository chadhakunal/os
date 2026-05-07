#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "types.h"

#define MAX_PATH_COPY 256
#define AT_FDCWD -100

DEFINE_SYSCALL4(openat, int, dirfd, const char *, user_path, uint64_t, flags, uint64_t, mode)
{
  char path[MAX_PATH_COPY];
  copy_string_from_user(path, user_path, MAX_PATH_COPY);

  // For now, only support absolute paths and AT_FDCWD
  // TODO: Implement relative path resolution with dirfd
  if (path[0] != '/' && dirfd != AT_FDCWD) {
    return -1;  // Not supported yet
  }

  struct file_t *file;
  int ret = vfs_open(path, flags, &file);
  if (ret != 0) {
    return -1;
  }

  int fd = alloc_fd(&current_task->file_table, file);
  if (fd < 0) {
    return -1;
  }

  return fd;
}
