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

/* readlinkat(dirfd, path, buf, bufsiz) — reads symlink target without following */
DEFINE_SYSCALL4(readlinkat,
                int,          dirfd,
                const char *, user_path,
                char *,       user_buf,
                size_t,       bufsiz)
{
  char path[MAX_PATH_COPY];
  copy_string_from_user(path, user_path, MAX_PATH_COPY);

  struct dentry_t *start = task_dirfd_to_dentry(dirfd);
  if (start == (struct dentry_t *)-1) return -EBADF;

  /*
   * We must NOT follow the final symlink here — that's the whole point of
   * readlink. vfs_resolve_path_at follows symlinks at intermediate components
   * but we need to resolve the path manually to avoid following the last one.
   *
   * Strategy: resolve the parent directory normally (which does follow any
   * symlinks in intermediate components), then do a single vfs_lookup on the
   * final component without following.
   */
  char parent_path[MAX_PATH_COPY], name[MAX_PATH_COPY];
  int len = str_len(path, MAX_PATH_COPY);
  int last_slash = -1;
  for (int i = 0; i < len; i++) {
    if (path[i] == '/') last_slash = i;
  }
  if (last_slash < 0) {
    parent_path[0] = '.'; parent_path[1] = '\0';
    strncpy(name, path, MAX_PATH_COPY);
  } else if (last_slash == 0) {
    parent_path[0] = '/'; parent_path[1] = '\0';
    strncpy(name, path + 1, MAX_PATH_COPY);
  } else {
    strncpy(parent_path, path, last_slash + 1);
    strncpy(name, path + last_slash + 1, MAX_PATH_COPY);
  }

  struct dentry_t *parent_dentry;
  if (vfs_resolve_path_at(parent_path, start, &parent_dentry,
                          VFS_RESOLVE_FOLLOW_ALL) < 0) {
    debugk("readlinkat: parent '%s' not found\n", parent_path);
    return -ENOENT;
  }

  struct dentry_t *link_dentry;
  if (vfs_lookup(name, parent_dentry, &link_dentry) < 0) {
    debugk("readlinkat: '%s' not found\n", name);
    return -ENOENT;
  }

  struct vnode_t *vnode = link_dentry->vnode->mounted_vnode
                          ? link_dentry->vnode->mounted_vnode
                          : link_dentry->vnode;

  char kbuf[MAX_PATH_COPY];
  int64_t ret = vfs_readlink(vnode, kbuf, sizeof(kbuf));
  if (ret < 0) {
    debugk("readlinkat: vfs_readlink failed: %lld\n", ret);
    return ret;
  }

  size_t copy_len = (size_t)ret < bufsiz ? (size_t)ret : bufsiz;
  copy_to_user(user_buf, kbuf, copy_len);
  return (int64_t)copy_len;
}
