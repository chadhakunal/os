#include "kernel/filesystem/devfs/devfs.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/drivers/tty.h"
#include "lib/list.h"

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
  tty_dentry->vnode = tty_vnode;
  tty_dentry->parent = root_vnode;

  list_append(&root_vnode->children_dentries, &tty_dentry->sibling_dentry);

  return root_vnode;
}

struct superblock_t *devfs_mount() {
  struct superblock_t *superblock = superblock_t_alloc();
  superblock->root_vnode = build_devfs(superblock);

  return superblock;
}
