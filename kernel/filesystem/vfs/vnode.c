#include "kernel/filesystem/vfs/vfs.h"
#include "lib/printk/printk.h"
#include "kernel/filesystem/mode.h"

void init_vnode(struct vnode_t *vnode, struct superblock_t *sb, uint32_t id, mode_t permission_mode, uint64_t size) {
  vnode->superblock = sb;
  vnode->ops = &sb->vnode_ops;
  vnode->id = id;
  vnode->refcount = 1;
  vnode->owner_uid = 0;
  vnode->owner_gid = 0;
  vnode->permission_mode = permission_mode;
  vnode->size = size;
  vnode->first_child_dentry = NULL;
  vnode->last_child_dentry = NULL;
  vnode->fs_private_vnode = NULL;
}

void vfs_print_vnode(struct vnode_t *vnode) {
  if (vnode == NULL) {
    printk("[vnode: NULL]\n");
    return;
  }
  printk("[vnode id=%d, size=%lld, refcount=%d, uid=%d, gid=%d, mode=0x%x%s, first_child=%p, fs_private=%p]\n",
         vnode->id,
         vnode->size,
         vnode->refcount,
         vnode->owner_uid,
         vnode->owner_gid,
         vnode->permission_mode,
         IS_DIR(vnode->permission_mode) ? " (DIR)" : "",
         vnode->first_child_dentry,
         vnode->fs_private_vnode);
}
