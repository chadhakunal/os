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
    ret = vfs_lookup(current_name, curr_dentry, &next_dentry);
    if (ret != 0 || next_dentry == NULL) {
      return ret;
    }
    curr_dentry = next_dentry;
    name_len = str_tok_no_delim(&current_path, current_name, '/', 256);
  }
  *out = curr_dentry;
  return 0;
}

int32_t vfs_lookup(const char *name, struct dentry_t *parent_dir, struct dentry_t **out) {
  printk("vfs_lookup: looking up '%s' in parent '%s'\n", name, parent_dir->name);
  printk("  parent mode=0x%x, IS_DIR=%d\n",
         parent_dir->vnode->permission_mode, IS_DIR(parent_dir->vnode->permission_mode));

  if (parent_dir == NULL) {
    panic("vfs_lookup: parent_dir is NULL\n");
  }

  if (!IS_DIR(parent_dir->vnode->permission_mode)) {
    panic("vfs_lookup: parent_dir is not a directory\n");
  }

  /*
  * Note: This is pre- vfs abstraction, at this moment tarfs is only being used
  * Thus everything will be in memory, so tarfs has no fs specific handlers. This
  * needs to be abstracted to allow for other fs to hook into it, ie inode->i_ops->lookup()
  * First search for the dentry in the in memory dcache (should have negative dentries, and positive dentries)
  * if not found as neg dentry or pos dentry must ask underlying fs via method above
  */

  list_for_each(&parent_dir->vnode->children_dentries, pos) {
    struct dentry_t *dentry = container_of(pos, struct dentry_t, sibling_dentry);
    if (strncmp(dentry->name, name) == 0) {
      *out = dentry;
      return 0;
    }
  }
  *out = NULL;
  return -1;
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
