#include "kernel/filesystem/tarfs/tarfs.h"
#include "kernel/filesystem/tarfs/tarfs_parser.h"
#include "kernel/filesystem/vfs.h"
#include "types.h"
#include "lib/printk/printk.h"

struct superblock_t *tarfs_mount(void *data, uint64_t size) {
  struct superblock_t *tarfs_superblock = superblock_t_alloc();
  parse_tar(data, size, tarfs_superblock);
  printk("tarfs_mount: %lld, refcount: %lld\n", tarfs_superblock->root_vnode->permission_mode, tarfs_superblock->root_vnode->refcount);
  return tarfs_superblock;
}
