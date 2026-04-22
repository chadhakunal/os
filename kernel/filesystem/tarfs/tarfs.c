#include "kernel/filesystem/tarfs/tarfs.h"
#include "kernel/filesystem/tarfs/tarfs_parser.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "types.h"
#include "lib/printk/printk.h"
#include "lib/string.h"
#include "kernel/memory/page_allocator.h"
#include "virtual_memory_init.h"

int64_t tarfs_fill_page(struct vnode_t *vnode, size_t offset, void **phys_page) {
  struct tarfs_vnode_t *tarfs_vnode = vnode->fs_private_vnode;
  
  if (offset >= vnode->size) {
    return 0;
  }
  size_t size = DEFAULT_PAGE_SIZE;
  if (offset + size > vnode->size) {
    size = vnode->size - offset;
  }
  void *new_page = get_page(true);
  uint8_t *data_start = (uint8_t *)tarfs_vnode->data + offset;
  memcpy(PHYS_TO_VIRT(new_page), data_start, size);
  *phys_page = new_page;
  return 0;
}

int64_t tarfs_vnode_lookup(const char *name, struct vnode_t *parent_dir, struct dentry_t **out) {
  // All tarfs shouldve been loaded into memory at mount time, thus if vfs reaches out to this func, the entry does not exist
  *out = NULL;
  return -1;
}


struct vnode_t *tarfs_alloc_vnode(struct superblock_t *superblock) {
  struct vnode_t *vnode = vnode_t_alloc();
  vfs_init_vnode(vnode, superblock, ((struct tarfs_superblock_t *)superblock->private_data)->last_vnode_id);
  ((struct tarfs_superblock_t *)superblock->private_data)->last_vnode_id += 1;
  vnode->address_space = address_space_t_alloc();
  vnode->address_space->num_pages_used = 0;
  vnode->address_space->page_cache_list.next = &vnode->address_space->page_cache_list;
  vnode->address_space->page_cache_list.prev = &vnode->address_space->page_cache_list;
  vnode->fs_private_vnode = tarfs_vnode_t_alloc();
  vnode->address_space_ops = &superblock.address_space_ops;
  return vnode;
}

struct superblock_t *tarfs_mount(void *data, uint64_t size) {
  struct superblock_t *superblock = superblock_t_alloc();
  superblock->private_data = (void *)tarfs_superblock_t_alloc();
  ((struct tarfs_superblock_t *)superblock->private_data)->last_vnode_id = 0;
  superblock->vnode_ops.lookup = tarfs_vnode_lookup;
  superblock->address_space_ops.fill_page = tarfs_fill_page;

  parse_tar(data, size, superblock);
  return superblock;
}

