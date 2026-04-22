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
  list_for_each(&vnode->address_space.page_cache_list, pos) {
    struct page_cache_entry_t *page_cache_entry = container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
    if (page_cache_entry->offset == page_offset) {
      return page_cache_entry->physical_page;
    }
  }
  // Couldnt find a page cache entry, pull it in
  void *phys_page = get_page(true);
  void *virt_page = PHYS_TO_VIRT(phys_page);
  int64_t ret = vnode->ops->read(vnode, virt_page, page_offset, DEFAULT_PAGE_SIZE);
  if (ret < 0) {
    printk("ERROR with reading from inode\n");
    free_page(phys_page);
    return NULL;
  }

  // Add to cache
  struct page_cache_entry_t *page_cache_entry = page_cache_entry_t_alloc();
  page_cache_entry->offset = page_offset;
  page_cache_entry->physical_page = phys_page;
  page_cache_entry->refcount = 1;
  page_cache_entry->dirty = false;
  list_append(&vnode->address_space.page_cache_list, &page_cache_entry->sibling_page_cache_entry);

  return phys_page;
}
