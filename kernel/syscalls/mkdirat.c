#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "types.h"

#define AT_FDCWD -100

/* Split path into parent path and final component.
   e.g. "/mnt/foo" -> parent="/mnt", name="foo"
        "foo"      -> parent=".",    name="foo" */
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

DEFINE_SYSCALL3(mkdirat, int, dirfd, const char *, user_path, uint32_t, mode) {
  (void)dirfd; (void)mode;

  char path[256];
  copy_string_from_user(path, user_path, 256);

  char parent_path[256], name[256];
  split_path(path, parent_path, name);

  struct dentry_t *parent_dentry;
  if (vfs_resolve_path(parent_path, &parent_dentry) < 0)
    return -1;

  struct vnode_t *parent_vnode = parent_dentry->vnode->mounted_vnode
                                 ? parent_dentry->vnode->mounted_vnode
                                 : parent_dentry->vnode;

  struct dentry_t *new_dentry;
  int64_t ret = vfs_mkdir(name, parent_vnode, &new_dentry);
  if (ret < 0)
    return (int)ret;
  if (new_dentry != NULL) {
    new_dentry->parent = parent_dentry;
    list_append(&parent_vnode->children_dentries, &new_dentry->sibling_dentry);
  }
  return 0;
}
