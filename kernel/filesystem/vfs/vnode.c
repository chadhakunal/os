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
  vnode->children_dentries.next = &vnode->children_dentries;
  vnode->children_dentries.prev = &vnode->children_dentries;
  vnode->address_space.num_pages_used = 0;
  vnode->address_space.page_cache_list.next = &vnode->address_space.page_cache_list;
  vnode->address_space.page_cache_list.prev = &vnode->address_space.page_cache_list;
  vnode->fs_private_vnode = NULL;
}

void vfs_print_vnode(struct vnode_t *vnode) {
  if (vnode == NULL) {
    printk("[vnode: NULL]\n");
    return;
  }
  printk("[vnode id=%d, size=%lld, refcount=%d, uid=%d, gid=%d, mode=0x%x%s, fs_private=%p]\n",
         vnode->id,
         vnode->size,
         vnode->refcount,
         vnode->owner_uid,
         vnode->owner_gid,
         vnode->permission_mode,
         IS_DIR(vnode->permission_mode) ? " (DIR)" : "",
         vnode->fs_private_vnode);
}
