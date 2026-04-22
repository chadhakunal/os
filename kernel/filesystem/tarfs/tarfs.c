#include "kernel/filesystem/tarfs/tarfs.h"
#include "kernel/filesystem/tarfs/tarfs_parser.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "types.h"
#include "lib/printk/printk.h"
#include "lib/string.h"

struct superblock_t *tarfs_mount(void *data, uint64_t size) {
  struct superblock_t *tarfs_superblock = superblock_t_alloc();
  tarfs_superblock->vnode_ops.read = tarfs_vnode_read;
  tarfs_superblock->vnode_ops.lookup = tarfs_vnode_lookup;
  parse_tar(data, size, tarfs_superblock);
  return tarfs_superblock;
}

int64_t tarfs_vnode_read(struct vnode_t *vnode, void *buffer, uint64_t offset, uint64_t size) {
  struct tarfs_vnode_t *tarfs_vnode = vnode->fs_private_vnode;

  if (offset >= vnode->size) {
    return 0;
  }

  if (offset + size > vnode->size) {
    size = vnode->size - offset;
  }

  uint8_t *data_start = (uint8_t *)tarfs_vnode->data + offset;
  memcpy(buffer, data_start, size);
  return (int64_t)size;
}


int64_t tarfs_vnode_lookup(const char *name, struct vnode_t *parent_dir, struct dentry_t **out) {
  // All tarfs shouldve been loaded into memory at mount time, thus if vfs reaches out to this func, the entry does not exist
  *out = NULL;
  return -1;
}
