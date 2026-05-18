#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#define DEBUG 0
#include "lib/printk/printk.h"
#include "errno.h"

#define MAX_PATH_COPY      256
#define AT_SYMLINK_FOLLOW  0x400

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
  char old_path[MAX_PATH_COPY], new_path[MAX_PATH_COPY];
  char old_name[MAX_PATH_COPY], old_parent_path[MAX_PATH_COPY];
  char new_name[MAX_PATH_COPY], new_parent_path[MAX_PATH_COPY];
  copy_string_from_user(old_path, user_old_path, MAX_PATH_COPY);
  copy_string_from_user(new_path, user_new_path, MAX_PATH_COPY);

  struct dentry_t *old_start = task_dirfd_to_dentry(old_dirfd);
  if (old_start == (struct dentry_t *)-1) return -EBADF;
  struct dentry_t *new_start = task_dirfd_to_dentry(new_dirfd);
  if (new_start == (struct dentry_t *)-1) return -EBADF;

  /* Resolve the source: which vnode to link and under which parent.
   *
   * AT_SYMLINK_FOLLOW: follow the final symlink — resolve the full old path
   * to the real target, then derive name and parent from that dentry.
   *
   * Without the flag: resolve only the parent directory; the link source is
   * the symlink inode itself (old_name inside old_parent). */
  struct vnode_t *old_parent;
  const char *src_name;

  if (flags & AT_SYMLINK_FOLLOW) {
    struct dentry_t *old_dentry;
    if (vfs_resolve_path_at(old_path, old_start, &old_dentry,
                            VFS_RESOLVE_FOLLOW_ALL) < 0) {
      debugk("linkat: old path '%s' not found\n", old_path);
      return -ENOENT;
    }
    if (old_dentry->parent == NULL) return -ENOENT;
    src_name = old_dentry->name;
    struct vnode_t *pv = old_dentry->parent->vnode;
    old_parent = pv->mounted_vnode ? pv->mounted_vnode : pv;
  } else {
    split_path(old_path, old_parent_path, old_name);
    struct dentry_t *old_parent_dentry;
    if (vfs_resolve_path_at(old_parent_path, old_start, &old_parent_dentry,
                            VFS_RESOLVE_FOLLOW_ALL) < 0) {
      debugk("linkat: old parent '%s' not found\n", old_parent_path);
      return -ENOENT;
    }
    struct vnode_t *pv = old_parent_dentry->vnode;
    old_parent = pv->mounted_vnode ? pv->mounted_vnode : pv;
    src_name = old_name;
  }

  /* Resolve the destination parent directory. */
  split_path(new_path, new_parent_path, new_name);
  struct dentry_t *new_parent_dentry;
  if (vfs_resolve_path_at(new_parent_path, new_start, &new_parent_dentry,
                          VFS_RESOLVE_FOLLOW_ALL) < 0) {
    debugk("linkat: new parent '%s' not found\n", new_parent_path);
    return -ENOENT;
  }
  struct vnode_t *npv = new_parent_dentry->vnode;
  struct vnode_t *new_parent = npv->mounted_vnode ? npv->mounted_vnode : npv;

  int64_t ret = vfs_link(src_name, old_parent, new_name, new_parent);
  if (ret < 0)
    debugk("linkat: link '%s' -> '%s' failed: %lld\n", old_path, new_path, ret);
  return ret;
}
