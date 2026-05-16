#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#define DEBUG 0
#include "lib/printk/printk.h"
#include "errno.h"

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

DEFINE_SYSCALL5(linkat,
                int,          old_dirfd,
                const char *, user_old_path,
                int,          new_dirfd,
                const char *, user_new_path,
                int,          flags)
{
  (void)flags;

  char old_path[MAX_PATH_COPY], new_path[MAX_PATH_COPY];
  copy_string_from_user(old_path, user_old_path, MAX_PATH_COPY);
  copy_string_from_user(new_path, user_new_path, MAX_PATH_COPY);

  struct dentry_t *old_start = task_dirfd_to_dentry(old_dirfd);
  if (old_start == (struct dentry_t *)-1) return -EBADF;
  struct dentry_t *new_start = task_dirfd_to_dentry(new_dirfd);
  if (new_start == (struct dentry_t *)-1) return -EBADF;

  char old_parent_path[MAX_PATH_COPY], old_name[MAX_PATH_COPY];
  split_path(old_path, old_parent_path, old_name);

  struct dentry_t *old_parent_dentry;
  if (vfs_resolve_path_at(old_parent_path, old_start, &old_parent_dentry,
                          VFS_RESOLVE_FOLLOW_ALL) < 0) {
    debugk("linkat: old parent '%s' not found\n", old_parent_path);
    return -ENOENT;
  }
  struct vnode_t *old_parent = old_parent_dentry->vnode->mounted_vnode
                               ? old_parent_dentry->vnode->mounted_vnode
                               : old_parent_dentry->vnode;

  char new_parent_path[MAX_PATH_COPY], new_name[MAX_PATH_COPY];
  split_path(new_path, new_parent_path, new_name);

  struct dentry_t *new_parent_dentry;
  if (vfs_resolve_path_at(new_parent_path, new_start, &new_parent_dentry,
                          VFS_RESOLVE_FOLLOW_ALL) < 0) {
    debugk("linkat: new parent '%s' not found\n", new_parent_path);
    return -ENOENT;
  }
  struct vnode_t *new_parent = new_parent_dentry->vnode->mounted_vnode
                               ? new_parent_dentry->vnode->mounted_vnode
                               : new_parent_dentry->vnode;

  int64_t ret = vfs_link(old_name, old_parent, new_name, new_parent);
  if (ret < 0)
    debugk("linkat: link '%s' -> '%s' failed: %lld\n", old_path, new_path, ret);
  return ret;
}
