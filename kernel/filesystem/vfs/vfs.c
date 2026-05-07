#define DEBUG 0
#include "kernel/filesystem/vfs/vfs.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "kernel/panic.h"
#include "kernel/filesystem/mode.h"
#include "lib/list.h"
#include "kernel/memory/page_allocator.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/task/task.h"

int32_t vfs_resolve_path(const char *path, struct dentry_t **out) {
  debugk("vfs_resolve_path: path=%s\n", path);
  struct dentry_t *curr_dentry, *next_dentry;
  uint32_t ret;
  int name_len;
  char current_name[256];
  const char *current_path = path;
  if (path[0] == '/') {
    curr_dentry = base_mount->superblock->root_dentry;
    name_len = str_tok_no_delim(&current_path, current_name, '/', 256); // Strip the first /
  } else {
    // Relative path! we should attach the cwd path then perform the lookup with full path
    // ie start from curr_dentry = current_task->cwd;
    curr_dentry = current_task->cwd;
  }
  debugk("vfs_resolve_path: root_dentry=%p\n", curr_dentry);
  name_len = str_tok_no_delim(&current_path, current_name, '/', 256);
  debugk("vfs_resolve_path: first component='%s', len=%d\n", current_name, name_len);
  while (name_len > 0) {
    debugk("vfs_resolve_path: looking up '%s' in vnode=%p\n", current_name, curr_dentry->vnode);
    debugk("vfs_resolve_path: mounted_vnode=%p\n", curr_dentry->vnode->mounted_vnode);
    if (!strncmp(current_name, ".")) {
      // keep the current dentry the same
      curr_dentry = curr_dentry; // nop
    } else if (!strncmp(current_name, "..")) {
      curr_dentry = curr_dentry->parent;
    } else {
      ret = vfs_lookup(current_name, curr_dentry->vnode->mounted_vnode != NULL ? curr_dentry->vnode->mounted_vnode : curr_dentry->vnode, &next_dentry);
      debugk("vfs_resolve_path: vfs_lookup returned %d, next_dentry=%p\n", ret, next_dentry);
      if (ret != 0 || next_dentry == NULL) {
        debugk("vfs_resolve_path: lookup failed, returning %d\n", ret);
        return ret;
      }
      curr_dentry = next_dentry;
      debugk("vfs_resolve_path: next component='%s', len=%d\n", current_name, name_len);
    }
    // Move to next portion of path
    name_len = str_tok_no_delim(&current_path, current_name, '/', 256);
  }
  debugk("vfs_resolve_path: success, returning dentry=%p\n", curr_dentry);
  *out = curr_dentry;
  return 0;
}


void *vfs_get_page(struct vnode_t *vnode, size_t offset, int flags){
  uint64_t page_offset = offset & ~(DEFAULT_PAGE_SIZE - 1);

  list_for_each(&vnode->address_space->page_cache_list, pos) {
    struct page_cache_entry_t *page_cache_entry = container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
    if (page_cache_entry->offset == page_offset) {
      if (flags & VFS_PAGE_REF) {
        page_cache_entry->refcount++;
      }
      return page_cache_entry->physical_page;
    }
  }

  void *phys_page;
  int ret = vnode->address_space->address_space_ops->fill_page(vnode, offset, &phys_page);
  if (ret < 0) {
    debugk("vfs_get_page: Error filling page with address_space_ops\n");
    return NULL;
  }

  struct page_cache_entry_t *page_cache_entry = page_cache_entry_t_alloc();
  page_cache_entry->offset = page_offset;
  page_cache_entry->physical_page = phys_page;
  page_cache_entry->refcount = (flags & VFS_PAGE_REF) ? 1 : 0;
  page_cache_entry->dirty = false;
  list_append(&vnode->address_space->page_cache_list, &page_cache_entry->sibling_page_cache_entry);

  return phys_page;
}

struct file_t *vfs_init_file(struct vnode_t *vnode, int flags) {
  struct file_t *file = file_t_alloc();
  file->vnode = vnode;
  file->file_ops = vnode->file_ops;
  file->offset = 0;
  file->refcount = 1;  // Start with refcount = 1
  file->flags = flags;
  return file;
}

int64_t vfs_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  debugk("vfs_read: file=%p, offset=%lu, size=%lu\n", file, offset, size);

  if (file == NULL) {
    panic("vfs_read: File is NULL\n");
  }

  debugk("  file->vnode=%p\n", file->vnode);
  debugk("  file->vnode->address_space=%p\n", file->vnode->address_space);
  debugk("  file->file_ops=%p\n", file->file_ops);

  if (file->vnode->address_space == NULL || file->vnode->address_space->address_space_ops == NULL || file->vnode->address_space->address_space_ops->fill_page == NULL) {
    debugk("  Using file_ops->read\n");
    if (file->file_ops == NULL || file->file_ops->read == NULL) {
      debugk("  ERROR: file_ops or read function is NULL\n");
      panic("vfs_read: file_ops or read function is NULL\n");
    }
    debugk("  file->file_ops->read=%p\n", file->file_ops->read);
    return file->file_ops->read(file, offset, buffer, size);
  }

  // Address_space and fill page is available
  debugk("  Using vfs_vnode_read\n");
  int ret = vfs_vnode_read(file->vnode, buffer, size, offset);
  debugk("  vfs_vnode_read returned %ld\n", ret);

  return ret;
}

int64_t vfs_write(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  debugk("vfs_write: file=%p, offset=%lu, size=%lu\n", file, offset, size);
  if (file == NULL) {
    panic("vfs_write: file is null");
  }

  if (file->file_ops == NULL || file->file_ops->write == NULL) {
    debugk("  ERROR: file_ops or write function is NULL\n");
    panic("vfs_write: file_ops or write function is NULL\n");
  }

  return file->file_ops->write(file, offset, buffer, size);
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

void vfs_address_space_inc_ref(uint64_t vaddr_start, uint64_t vaddr_end, uint64_t offset, struct address_space_t *address_space) {
  for (uint64_t va = vaddr_start; va < vaddr_end; va += DEFAULT_PAGE_SIZE) {
    uint64_t page_index = (va - vaddr_start) / DEFAULT_PAGE_SIZE;
    uint64_t file_page_offset = (page_index * DEFAULT_PAGE_SIZE + offset) & ~(DEFAULT_PAGE_SIZE - 1);

    list_for_each(&address_space->page_cache_list, pos) {
      struct page_cache_entry_t *entry = container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
      if (entry->offset == file_page_offset) {
        debugk("    Page cache entry at file offset %llx: refcount %llu -> %llu\n",
               file_page_offset, entry->refcount, entry->refcount + 1);
        entry->refcount++;
        break;
      }
    }
  }
}

void vfs_address_space_drop_ref(uint64_t vaddr_start, uint64_t vaddr_end, uint64_t offset, struct address_space_t *address_space) {
  for (uint64_t va = vaddr_start; va < vaddr_end; va += DEFAULT_PAGE_SIZE) {
    uint64_t page_index = (va - vaddr_start) / DEFAULT_PAGE_SIZE;
    uint64_t file_page_offset = (page_index * DEFAULT_PAGE_SIZE + offset) & ~(DEFAULT_PAGE_SIZE - 1);

    list_for_each(&address_space->page_cache_list, pos) {
      struct page_cache_entry_t *entry = container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
      if (entry->offset == file_page_offset) {
        debugk("    Page cache entry at file offset %llx: refcount %llu -> %llu\n",
               file_page_offset, entry->refcount, entry->refcount - 1);
        entry->refcount--;
        if (entry->refcount == 0) {
          debugk("    Freeing page cache entry phys=%p\n", entry->physical_page);
          free_page(entry->physical_page);
          list_remove(&entry->sibling_page_cache_entry);
          page_cache_entry_t_free(entry);
        }
        break;
      }
    }
  }
}
