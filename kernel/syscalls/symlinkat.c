#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
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

/* symlinkat(target, new_dirfd, linkpath) */
DEFINE_SYSCALL3(symlinkat,
                const char *, user_target,
                int,          new_dirfd,
                const char *, user_linkpath)
{
  char target[MAX_PATH_COPY], linkpath[MAX_PATH_COPY];
  copy_string_from_user(target,   user_target,   MAX_PATH_COPY);
  copy_string_from_user(linkpath, user_linkpath, MAX_PATH_COPY);

  struct dentry_t *start = task_dirfd_to_dentry(new_dirfd);
  if (start == (struct dentry_t *)-1) return -EBADF;

  char parent_path[MAX_PATH_COPY], name[MAX_PATH_COPY];
  split_path(linkpath, parent_path, name);

  struct dentry_t *parent_dentry;
  if (vfs_resolve_path_at(parent_path, start, &parent_dentry) < 0) {
    printk("symlinkat: parent '%s' not found\n", parent_path);
    return -ENOENT;
  }

  struct vnode_t *parent_vnode = parent_dentry->vnode->mounted_vnode
                                 ? parent_dentry->vnode->mounted_vnode
                                 : parent_dentry->vnode;

  struct dentry_t *new_dentry;
  int64_t ret = vfs_symlink(target, name, parent_vnode, &new_dentry);
  if (ret < 0) {
    printk("symlinkat: vfs_symlink('%s' -> '%s') failed: %lld\n", linkpath, target, ret);
    return ret;
  }

  if (new_dentry != NULL) {
    new_dentry->parent = parent_dentry;
    list_append(&parent_vnode->children_dentries, &new_dentry->sibling_dentry);
  }
  return 0;
}
