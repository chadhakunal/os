#include "kernel/filesystem/vfs/vfs.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "panic.h"
#include "kernel/filesystem/mode.h"
#include "lib/list.h"

#define READ_EXECUTE_PERM PERM_RUSR | PERM_XUSR | PERM_RGRP | PERM_XGRP | PERM_ROTH | PERM_XOTH

int32_t vfs_resolve_path(const char *path, struct dentry_t **out) {
  struct dentry_t *curr_dentry = base_mount->superblock->root_dentry;
  struct dentry_t *next_dentry;
  uint32_t ret;
  const char *current_path = path;
  char current_name[256];
  int name_len = str_tok_no_delim(&current_path, current_name, '/', 256);
  name_len = str_tok_no_delim(&current_path, current_name, '/', 256);
  while (name_len > 0) {
    ret = vfs_lookup(current_name, curr_dentry, &next_dentry);
    if (ret != 0 || next_dentry == NULL) {
      return ret;
    }
    curr_dentry = next_dentry;
    name_len = str_tok_no_delim(&current_path, current_name, '/', 256);
  }
  *out = curr_dentry;
  return 0;
}

int32_t vfs_lookup(const char *name, struct dentry_t *parent_dir, struct dentry_t **out) {
  if (parent_dir == NULL) {
    panic("vfs_lookup: parent_dir is NULL\n");
  }

  if (!IS_DIR(parent_dir->vnode->permission_mode)) {
    panic("vfs_lookup: parent_dir is not a directory\n");
  }

  /*
  * Note: This is pre- vfs abstraction, at this moment tarfs is only being used
  * Thus everything will be in memory, so tarfs has no fs specific handlers. This
  * needs to be abstracted to allow for other fs to hook into it, ie inode->i_ops->lookup()
  * First search for the dentry in the in memory dcache (should have negative dentries, and positive dentries)
  * if not found as neg dentry or pos dentry must ask underlying fs via method above
  */

  list_for_each(parent_dir->vnode->children_dentries, pos) {
    struct dentry_t *dentry = container_of(pos, struct dentry_t, sibling_dentry);
    if (strncmp(dentry->name, name) == 0) {
      *out = dentry;
      return 0;
    }
  }
  *out = NULL;
  return -1;
}
