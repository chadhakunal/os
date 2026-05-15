#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "types.h"

#define MAX_PATH_COPY 256
#define AT_FDCWD -100

static void split_path(const char *path, char *parent, char *name) {
  int len = str_len(path, MAX_PATH_COPY);
  int last_slash = -1;
  for (int i = 0; i < len; i++) {
    if (path[i] == '/') last_slash = i;
  }
  if (last_slash < 0) {
    parent[0] = '.'; parent[1] = '\0';
    strncpy(name, path, MAX_PATH_COPY);
  } else if (last_slash == 0) {
    parent[0] = '/'; parent[1] = '\0';
    strncpy(name, path + 1, MAX_PATH_COPY);
  } else {
    strncpy(parent, path, last_slash + 1);
    strncpy(name, path + last_slash + 1, MAX_PATH_COPY);
  }
}

DEFINE_SYSCALL4(openat, int, dirfd, const char *, user_path, uint64_t, flags, uint64_t, mode)
{
  (void)mode;
  char path[MAX_PATH_COPY];
  copy_string_from_user(path, user_path, MAX_PATH_COPY);

  if (path[0] != '/' && dirfd != AT_FDCWD) {
    return -1;
  }

  if (flags & O_CREAT) {
    char parent_path[MAX_PATH_COPY], name[MAX_PATH_COPY];
    split_path(path, parent_path, name);

    struct dentry_t *parent_dentry;
    if (vfs_resolve_path(parent_path, &parent_dentry) < 0)
      return -1;

    struct vnode_t *parent_vnode = parent_dentry->vnode->mounted_vnode
                                   ? parent_dentry->vnode->mounted_vnode
                                   : parent_dentry->vnode;

    struct dentry_t *new_dentry;
    int ret = vfs_create(name, parent_vnode, &new_dentry);
    if (ret < 0)
      return -1;

    if (new_dentry != NULL) {
      new_dentry->parent = parent_dentry;
      list_append(&parent_vnode->children_dentries, &new_dentry->sibling_dentry);
    }

    int fd = alloc_fd(&current_task->file_table, vfs_init_file(new_dentry->vnode, flags));
    return fd;
  }

  struct file_t *file;
  int ret = vfs_open(path, flags, &file);
  if (ret != 0) {
    return -1;
  }

  // Handle O_TRUNC: truncate file to 0 length
  if (flags & O_TRUNC) {
    // TODO: implement truncate operation
    // For now, we can set offset to 0 and rely on writes to overwrite
    file->offset = 0;
  }

  int fd = alloc_fd(&current_task->file_table, file);
  if (fd < 0) {
    return -1;
  }

  return fd;
}
