#include "kernel/filesystem/devfs/devfs.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/drivers/tty.h"
#include "lib/list.h"
#include "lib/string.h"

struct vnode_t *build_devfs(struct superblock_t *superblock) {
  struct vnode_t *root_vnode = vnode_t_alloc();
  uint32_t id = 0;
  vfs_init_vnode(root_vnode, superblock, id++);
  root_vnode->permission_mode = RW_PERM | S_IFDIR;
  
  struct vnode_t *tty_vnode = vnode_t_alloc();
  vfs_init_vnode(tty_vnode, superblock, id++);
  tty_vnode->permission_mode = RW_PERM | S_IFCHR;

  // Set up TTY file operations
  tty_vnode->file_ops = &tty_driver.file_ops;

  struct dentry_t *tty_dentry = dentry_t_alloc();
  strncpy(tty_dentry->name, "tty", 256);
  tty_dentry->vnode = tty_vnode;
  tty_dentry->parent = NULL;  // Will be set at mount time

  list_append(&root_vnode->children_dentries, &tty_dentry->sibling_dentry);

  return root_vnode;
}

struct superblock_t *devfs_mount() {
  struct superblock_t *superblock = superblock_t_alloc();
  // devfs doesn't use superblock file_ops (devices override per-vnode)
  superblock->file_ops.read = NULL;
  superblock->file_ops.write = NULL;

  superblock->root_vnode = build_devfs(superblock);

  // Create root dentry for devfs
  struct dentry_t *root_dentry = dentry_t_alloc();
  strncpy(root_dentry->name, "dev", 256);
  root_dentry->vnode = superblock->root_vnode;
  root_dentry->parent = NULL;  // Will be set at mount time
  superblock->root_dentry = root_dentry;

  return superblock;
}
