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

  char resolved_path[MAX_PATH_COPY];

  // If path is absolute, ignore dirfd
  if (path[0] == '/') {
    strncpy(resolved_path, path, MAX_PATH_COPY);
  }
  // If dirfd is AT_FDCWD, use path as-is (relative to CWD)
  else if (dirfd == AT_FDCWD) {
    strncpy(resolved_path, path, MAX_PATH_COPY);
  }
  // Otherwise, resolve relative to dirfd
  else {
    struct file_t *dir_file = find_file(&current_task->file_table, dirfd);
    if (!dir_file || !dir_file->dentry) {
      return -1;
    }

    // Build path: dirfd's path + "/" + relative path
    vfs_get_absolute_path(dir_file->dentry, resolved_path, MAX_PATH_COPY);
    int dir_len = str_len(resolved_path, MAX_PATH_COPY);

    // Add "/" if not already there
    if (dir_len > 0 && resolved_path[dir_len - 1] != '/') {
      resolved_path[dir_len] = '/';
      dir_len++;
    }

    // Append relative path
    strncpy(resolved_path + dir_len, path, MAX_PATH_COPY - dir_len);
  }

  struct file_t *file;
  int ret = vfs_open(resolved_path, flags, &file);
  if (ret != 0) {
    return -1;
  }

  int fd = alloc_fd(&current_task->file_table, file);
  if (fd < 0) {
    return -1;
  }

  return fd;
}
