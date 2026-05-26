#include "kernel/filesystem/tarfs/tarfs.h"
#include "kernel/filesystem/tarfs/tarfs_parser.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "types.h"
#include "lib/printk/printk.h"
#include "lib/string.h"
#include "kernel/memory/page_allocator.h"
#include "arch/riscv64/virtual_memory_init.h"

int64_t tarfs_fill_page(struct vnode_t *vnode, size_t offset, void **phys_page) {
  struct tarfs_vnode_t *tarfs_vnode = vnode->fs_private_vnode;

  if (offset >= vnode->size) {
    return 0;
  }
  size_t size = DEFAULT_PAGE_SIZE;
  if (offset + size > vnode->size) {
    size = vnode->size - offset;
  }
  void *new_page = get_zero_page(true);
  if (new_page == NULL) {
    return -1;
  }
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

int64_t tarfs_readdir(struct vnode_t *dir, uint32_t index, struct dentry_t **out) {
  uint32_t i = 0;
  list_for_each(&dir->children_dentries, pos) {
    if (i == index) {
      *out = container_of(pos, struct dentry_t, sibling_dentry);
      return 0;
    }
    i++;
  }
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
  vnode->address_space->address_space_ops = &superblock->address_space_ops;
  return vnode;
}

static int64_t tarfs_statfs(struct superblock_t *sb, struct vfs_statfs *buf) {
  (void)sb;
  buf->f_bsize   = 4096;
  buf->f_blocks  = 0;
  buf->f_bfree   = 0;
  buf->f_bavail  = 0;
  buf->f_files   = 0;
  buf->f_ffree   = 0;
  buf->f_namelen = 255;
  buf->f_frsize  = 4096;
  return 0;
}

struct superblock_t *tarfs_mount(void *data, uint64_t size) {
  struct superblock_t *superblock = superblock_t_alloc();
  superblock->private_data = (void *)tarfs_superblock_t_alloc();
  ((struct tarfs_superblock_t *)superblock->private_data)->last_vnode_id = 0;
  superblock->vnode_ops.lookup = tarfs_vnode_lookup;
  superblock->vnode_ops.readdir = tarfs_readdir;
  superblock->address_space_ops.fill_page = tarfs_fill_page;
  superblock->superblock_ops.statfs = tarfs_statfs;
  superblock->file_ops.read = NULL;
  superblock->file_ops.write = NULL;

  parse_tar(data, size, superblock);
  return superblock;
}

