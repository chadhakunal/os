#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/mode.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#define DEBUG 0
#include "lib/printk/printk.h"
#include "types.h"
#include "errno.h"
#include "kernel/task/task.h"

#define MAX_PATH_COPY 256

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
  char path[MAX_PATH_COPY];
  copy_string_from_user(path, user_path, MAX_PATH_COPY);

  struct dentry_t *start = task_dirfd_to_dentry((int)dirfd);
  if (start == (struct dentry_t *)-1)
    return -EBADF;

  if (flags & O_CREAT) {
    char parent_path[MAX_PATH_COPY], name[MAX_PATH_COPY];
    split_path(path, parent_path, name);

    struct dentry_t *parent_dentry;
    int parent_ret = vfs_resolve_path_at(parent_path, start, &parent_dentry,
                                         VFS_RESOLVE_FOLLOW_ALL);
    if (parent_ret < 0)
      return parent_ret;

    struct vnode_t *parent_vnode = parent_dentry->vnode->mounted_vnode
                                   ? parent_dentry->vnode->mounted_vnode
                                   : parent_dentry->vnode;

    uint32_t create_mode = (uint32_t)mode & 0777u;
    if (create_mode == 0)
      create_mode = 0666u;
    create_mode = task_apply_umask(current_task, create_mode);

    struct dentry_t *new_dentry;
    int ret = vfs_create(name, parent_vnode, &new_dentry, create_mode);
    if (ret == -EEXIST && !(flags & O_EXCL))
      goto open_existing;
    if (ret < 0)
      return ret;

    if (new_dentry != NULL) {
      new_dentry->parent = parent_dentry;
      list_append(&parent_vnode->children_dentries, &new_dentry->sibling_dentry);
    }

    struct vnode_t *child_vnode = new_dentry->vnode->mounted_vnode
                                  ? new_dentry->vnode->mounted_vnode
                                  : new_dentry->vnode;
    if ((flags & O_DIRECTORY) && !IS_DIR(child_vnode->permission_mode))
      return -ENOTDIR;

    struct file_t *new_file = vfs_init_file(new_dentry->vnode, flags);
    new_file->dentry = new_dentry;
    int fd = alloc_fd(&current_task->file_table, new_file);
    if (fd >= 0 && (flags & O_CLOEXEC))
      vfs_file_set_close_on_exec(&current_task->file_table, fd);
    return fd;
  }

open_existing:;
  struct file_t *file;
  int ret = vfs_open_at(path, start, flags, &file);
  if (ret != 0)
    return ret;

  if (flags & O_TRUNC) {
    vfs_truncate(file->vnode, 0);
    file->offset = 0;
  }

  int fd = alloc_fd(&current_task->file_table, file);
  if (fd >= 0 && (flags & O_CLOEXEC))
    vfs_file_set_close_on_exec(&current_task->file_table, fd);
  return fd;
}
