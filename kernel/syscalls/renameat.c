#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "errno.h"

#define MAX_PATH_COPY 256
#define AT_FDCWD      -100

/*
 * Split a path into its parent directory path and the final component name.
 * e.g. "/foo/bar/baz" → parent="/foo/bar", name="baz"
 *      "baz"          → parent=".",        name="baz"
 */
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

DEFINE_SYSCALL4(renameat,
                int,          old_dirfd,
                const char *, user_old_path,
                int,          new_dirfd,
                const char *, user_new_path)
{
  (void)old_dirfd;
  (void)new_dirfd;

  char old_path[MAX_PATH_COPY], new_path[MAX_PATH_COPY];
  copy_string_from_user(old_path, user_old_path, MAX_PATH_COPY);
  copy_string_from_user(new_path, user_new_path, MAX_PATH_COPY);

  /* Resolve old parent directory and entry name. */
  char old_parent_path[MAX_PATH_COPY], old_name[MAX_PATH_COPY];
  split_path(old_path, old_parent_path, old_name);

  struct dentry_t *old_parent_dentry;
  if (vfs_resolve_path(old_parent_path, &old_parent_dentry) < 0) {
    printk("renameat: old parent '%s' not found\n", old_parent_path);
    return -ENOENT;
  }
  struct vnode_t *old_parent = old_parent_dentry->vnode->mounted_vnode
                               ? old_parent_dentry->vnode->mounted_vnode
                               : old_parent_dentry->vnode;

  /* Resolve new parent directory and entry name. */
  char new_parent_path[MAX_PATH_COPY], new_name[MAX_PATH_COPY];
  split_path(new_path, new_parent_path, new_name);

  struct dentry_t *new_parent_dentry;
  if (vfs_resolve_path(new_parent_path, &new_parent_dentry) < 0) {
    printk("renameat: new parent '%s' not found\n", new_parent_path);
    return -ENOENT;
  }
  struct vnode_t *new_parent = new_parent_dentry->vnode->mounted_vnode
                               ? new_parent_dentry->vnode->mounted_vnode
                               : new_parent_dentry->vnode;

  int64_t ret = vfs_rename(old_name, old_parent, new_name, new_parent);
  if (ret < 0)
    printk("renameat: rename '%s' -> '%s' failed: %lld\n", old_path, new_path, ret);
  return ret;
}
