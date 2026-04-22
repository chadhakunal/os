#include "kernel/filesystem/vfs/vfs.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "panic.h"
#include "kernel/filesystem/mode.h"
#include "lib/list.h"
#include "kernel/memory/page_allocator.h"
#include "virtual_memory_init.h"

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
    ret = vfs_lookup(current_name, curr_dentry->vnode, &next_dentry);
    if (ret != 0 || next_dentry == NULL) {
      return ret;
    }
    curr_dentry = next_dentry;
    name_len = str_tok_no_delim(&current_path, current_name, '/', 256);
  }
  *out = curr_dentry;
  return 0;
}


void *vfs_get_page(struct vnode_t *vnode, size_t offset){
  // Align offset to page boundary
  uint64_t page_offset = offset & ~(DEFAULT_PAGE_SIZE - 1);

  // Search for page in address space
  list_for_each(&vnode->address_space->page_cache_list, pos) {
    struct page_cache_entry_t *page_cache_entry = container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
    if (page_cache_entry->offset == page_offset) {
      return page_cache_entry->physical_page;
    }
  }
  // Couldnt find a page cache entry, pull it in
  void *phys_page;
  int ret = vnode->address_space->address_space_ops->fill_page(vnode, offset, &phys_page);
  if (ret < 0) {
    printk("vfs_get_page: Error filling page with address_space_ops\n");
    return NULL;
  }

  // Add to cache
  struct page_cache_entry_t *page_cache_entry = page_cache_entry_t_alloc();
  page_cache_entry->offset = page_offset;
  page_cache_entry->physical_page = phys_page;
  page_cache_entry->refcount = 1;
  page_cache_entry->dirty = false;
  list_append(&vnode->address_space->page_cache_list, &page_cache_entry->sibling_page_cache_entry);

  return phys_page;
}

struct file_t *vfs_init_file(struct vnode_t *vnode, int flags) {
  struct file_t *file = file_t_alloc();
  file->vnode = vnode;
  file->file_ops = &vnode->superblock->file_ops;
  file->offset = 0;
  file->refcount = 0;
  file->flags = flags;
  return file;
}

int64_t vfs_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  printk("vfs_read: file=%p, offset=%lu, size=%lu\n", file, offset, size);

  if (file == NULL) {
    panic("vfs_read: File is NULL\n");
  }

  printk("  file->vnode=%p\n", file->vnode);
  printk("  file->vnode->address_space=%p\n", file->vnode->address_space);

  if (file->vnode->address_space == NULL || file->vnode->address_space->address_space_ops->fill_page == NULL) {
    printk("  Using file_ops->read\n");
    return file->file_ops->read(file, offset, buffer, size);
  }

  // Address_space and fill page is available
  printk("  Using vfs_vnode_read\n");
  int ret = vfs_vnode_read(file->vnode, buffer, size, offset);
  printk("  vfs_vnode_read returned %ld\n", ret);

  return ret;
}

int64_t vfs_open(const char *path, int flags, struct file_t **file) {
  if (path == NULL || *path == 0) {
    panic("vfs_open: invalid path\n");
  }
  struct dentry_t *dentry;
  int ret = vfs_resolve_path(path, &dentry);
  if (ret < 0) {
    panic("vfs_open: something went wrong resolving path!\n");
  }
  
  *file = vfs_init_file(dentry->vnode, flags);
  return 0;
}

