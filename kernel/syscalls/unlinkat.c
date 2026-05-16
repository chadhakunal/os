#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "types.h"
#include "errno.h"

#define AT_REMOVEDIR    0x200

static void split_path(const char *path, char *parent, char *name) {
  int len = str_len(path, 256);
  int last_slash = -1;
  for (int i = 0; i < len; i++) {
    if (path[i] == '/') last_slash = i;
  }

  if (last_slash < 0) {
    parent[0] = '.'; parent[1] = '\0';
    strncpy(name, path, 256);
  } else if (last_slash == 0) {
    parent[0] = '/'; parent[1] = '\0';
    strncpy(name, path + 1, 256);
  } else {
    strncpy(parent, path, last_slash + 1);
    strncpy(name, path + last_slash + 1, 256);
  }
}

/* unlinkat(dirfd, path, flags)
   flags=0           -> unlink (remove file)
   flags=AT_REMOVEDIR -> rmdir (remove directory) */
DEFINE_SYSCALL3(unlinkat, int, dirfd, const char *, user_path, int, flags) {
  char path[256];
  copy_string_from_user(path, user_path, 256);

  struct dentry_t *start = task_dirfd_to_dentry(dirfd);
  if (start == (struct dentry_t *)-1)
    return -EBADF;

  char parent_path[256], name[256];
  split_path(path, parent_path, name);

  struct dentry_t *parent_dentry;
  if (vfs_resolve_path_at(parent_path, start, &parent_dentry) < 0)
    return -1;

  struct vnode_t *parent_vnode = parent_dentry->vnode->mounted_vnode
                                 ? parent_dentry->vnode->mounted_vnode
                                 : parent_dentry->vnode;

  int64_t ret;
  if (flags & AT_REMOVEDIR)
    ret = vfs_rmdir(name, parent_vnode);
  else
    ret = vfs_unlink(name, parent_vnode);

  if (ret < 0)
    return ret;

  /* Evict the now-deleted entry from the VFS dentry cache so future
   * lookups do not return a stale hit. */
  list_for_each(&parent_vnode->children_dentries, pos) {
    struct dentry_t *d = container_of(pos, struct dentry_t, sibling_dentry);
    if (strncmp(d->name, name) == 0) {
      list_remove(&d->sibling_dentry);
      dentry_t_free(d);
      break;
    }
  }

  return 0;
}
