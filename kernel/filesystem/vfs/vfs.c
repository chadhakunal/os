#define DEBUG 1
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/pipefs/pipe.h"
#include "kernel/filesystem/poll.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "kernel/panic.h"
#include "kernel/filesystem/mode.h"
#include "lib/list.h"
#include "kernel/memory/page_allocator.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/task/task.h"
#include "errno.h"

#define SYMLINK_FOLLOW_MAX 40

int32_t vfs_resolve_path_at(const char *path, struct dentry_t *start, struct dentry_t **out, uint32_t resolve_flags) {
  debugk("[vfs_resolve_path_at] path=%s\n", path);
  debugk("vfs_resolve_path_at: path=%s\n", path);

  /* Work on a local copy so we can restart on symlink follows. */
  char work[512];
  strncpy(work, path, sizeof(work) - 1);
  work[sizeof(work) - 1] = '\0';

  int follow_count = 0;
  struct dentry_t *curr_dentry;

restart:;
  const char *current_path = work;
  int32_t ret;
  int name_len;
  char current_name[256];

  if (work[0] == '/') {
    curr_dentry = base_mount->superblock->root_dentry;
    name_len = str_tok_no_delim(&current_path, current_name, '/', 256);
  } else {
    curr_dentry = (start != NULL) ? start : current_task->cwd;
  }

  debugk("vfs_resolve_path_at: starting dentry=%p\n", curr_dentry);
  name_len = str_tok_no_delim(&current_path, current_name, '/', 256);

  while (name_len > 0) {
    debugk("vfs_resolve_path_at: looking up '%s'\n", current_name);
    if (!strncmp(current_name, ".")) {
      /* stay */
    } else if (!strncmp(current_name, "..")) {
      if (curr_dentry->parent != NULL)
        curr_dentry = curr_dentry->parent;
    } else {
      struct dentry_t *next_dentry = NULL;
      debugk("[vfs_resolve] lookup '%s'\n", current_name);
      ret = vfs_lookup(current_name, curr_dentry, &next_dentry);
      if (ret != 0) {
        debugk("vfs_resolve_path_at: lookup failed, returning %d\n", ret);
        return ret;
      }
      curr_dentry = next_dentry;
      debugk("[vfs_resolve] found '%s' vnode=%p mode=0x%x\n",
             current_name,
             curr_dentry->vnode,
             curr_dentry->vnode ? curr_dentry->vnode->permission_mode : 0);

      /* Entering a directory requires execute (traverse) permission. */
      struct vnode_t *cv = curr_dentry->vnode->mounted_vnode
                           ? curr_dentry->vnode->mounted_vnode
                           : curr_dentry->vnode;
      if (IS_DIR(cv->permission_mode) && !(cv->permission_mode & PERM_XUSR))
        return -EACCES;

      /* Follow symlinks on intermediate components and on the final one. */
      struct vnode_t *v = curr_dentry->vnode->mounted_vnode
                          ? curr_dentry->vnode->mounted_vnode
                          : curr_dentry->vnode;
      if (IS_LNK(v->permission_mode)) {
        bool is_final = (*current_path == '\0');
        if (is_final && (resolve_flags & VFS_RESOLVE_NOFOLLOW_FINAL))
          ; /* stat/lstat on symlink: leave dentry at the link itself */
        else {
        if (++follow_count > SYMLINK_FOLLOW_MAX)
          return -ELOOP;

        /* Read the symlink target. */
        char link_target[512];
        int64_t lret = vfs_readlink(v, link_target, sizeof(link_target));
        if (lret < 0) return (int32_t)lret;

        /* If there are remaining path components, append them. */
        if (name_len > 0 && *current_path != '\0') {
          /* current_path points at the rest of the path after this component */
          char combined[512];
          int tlen = str_len(link_target, sizeof(link_target));
          memcpy(combined, link_target, tlen);
          combined[tlen] = '/';
          strncpy(combined + tlen + 1, current_path, sizeof(combined) - tlen - 2);
          combined[sizeof(combined) - 1] = '\0';
          strncpy(work, combined, sizeof(work) - 1);
        } else {
          strncpy(work, link_target, sizeof(work) - 1);
        }
        work[sizeof(work) - 1] = '\0';

        /* Relative symlinks resolve from the symlink's parent directory. */
        start = (work[0] == '/') ? NULL : curr_dentry->parent;
        goto restart;
        }
      }
    }
    name_len = str_tok_no_delim(&current_path, current_name, '/', 256);
  }

  debugk("vfs_resolve_path_at: success, returning dentry=%p\n", curr_dentry);
  *out = curr_dentry;
  return 0;
}

int32_t vfs_resolve_path(const char *path, struct dentry_t **out) {
  return vfs_resolve_path_at(path, NULL, out, VFS_RESOLVE_FOLLOW_ALL);
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
  file->dentry = NULL;
  file->file_ops = vnode->file_ops;
  file->offset = 0;
  file->refcount = 1;
  file->flags = flags;
  file->pipe = NULL;
  file->pipe_write_end = 0;
  vnode->refcount++;
  debugk("[vfs_init_file] file=%p vnode=%p file_ops=%p mode=0x%x\n",
         file, vnode, vnode->file_ops, vnode->permission_mode);
  return file;
}

int64_t vfs_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  debugk("vfs_read: file=%p, offset=%lu, size=%lu\n", file, offset, size);

  if (file == NULL) {
    panic("vfs_read: File is NULL\n");
  }

  if (file->pipe != NULL)
    return pipe_read(file->pipe, buffer, size);

  if (IS_DIR(file->vnode->permission_mode))
    return -EISDIR;

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

short vfs_poll(struct file_t *file, short events) {
  if (!file)
    return POLLNVAL;
  if (file->pipe)
    return pipe_poll(file->pipe, file->pipe_write_end, events);
  /* Regular files are always ready. */
  return events & (POLLIN | POLLOUT);
}

int64_t vfs_write(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  if (file == NULL)
    panic("vfs_write: file is null");

  if (file->pipe != NULL)
    return pipe_write(file->pipe, buffer, size);

  if (file->vnode->address_space != NULL &&
      file->vnode->address_space->address_space_ops != NULL &&
      file->vnode->address_space->address_space_ops->write_page != NULL) {
    return vfs_vnode_write(file->vnode, buffer, size, offset);
  }

  if (file->file_ops == NULL || file->file_ops->write == NULL)
    panic("vfs_write: no write path available\n");

  return file->file_ops->write(file, offset, buffer, size);
}

static int64_t vfs_open_dentry(struct dentry_t *dentry, int flags, struct file_t **file) {
  if (dentry->vnode == NULL)
    return -ENOENT;

  struct vnode_t *vnode = dentry->vnode->mounted_vnode ? dentry->vnode->mounted_vnode
                                                       : dentry->vnode;

  if ((flags & O_DIRECTORY) && !IS_DIR(vnode->permission_mode))
    return -ENOTDIR;

  /* Permission check — only applies to regular files and symlinks (not dirs). */
  if (!IS_DIR(vnode->permission_mode)) {
    int acc = flags & O_ACCMODE;
    if (acc == O_RDONLY && !(vnode->permission_mode & PERM_RUSR))
      return -EACCES;
    if ((acc == O_WRONLY || acc == O_RDWR) && !(vnode->permission_mode & PERM_WUSR))
      return -EACCES;
  }

  *file = vfs_init_file(vnode, flags);
  (*file)->dentry = dentry;
  return 0;
}

int64_t vfs_open_at(const char *path, struct dentry_t *start, int flags,
                    struct file_t **file) {
  if (path == NULL || *path == 0)
    panic("vfs_open_at: invalid path\n");

  struct dentry_t *dentry;
  int ret = vfs_resolve_path_at(path, start, &dentry, VFS_RESOLVE_FOLLOW_ALL);
  if (ret < 0)
    return ret;

  return vfs_open_dentry(dentry, flags, file);
}

int64_t vfs_open(const char *path, int flags, struct file_t **file) {
  return vfs_open_at(path, NULL, flags, file);
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

void vfs_flush_dirty_pages(struct vnode_t *vnode) {
  if (vnode == NULL || vnode->address_space == NULL)
    return;

  struct address_space_ops_t *as_ops =
      vnode->address_space->address_space_ops;
  if (as_ops == NULL || as_ops->write_page == NULL)
    return;

  list_for_each(&vnode->address_space->page_cache_list, pos) {
    struct page_cache_entry_t *entry =
        container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
    if (entry->dirty) {
      as_ops->write_page(vnode, entry->offset, entry->physical_page);
      entry->dirty = false;
    }
  }
}

void vfs_invalidate_page_cache(struct vnode_t *vnode) {
  if (vnode->address_space == NULL)
    return;

  struct list_node *pos = vnode->address_space->page_cache_list.next;
  while (pos != &vnode->address_space->page_cache_list) {
    struct page_cache_entry_t *entry =
      container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
    pos = pos->next;
    free_page(entry->physical_page);
    list_remove(&entry->sibling_page_cache_entry);
    page_cache_entry_t_free(entry);
  }
}

void vfs_address_space_drop_ref(uint64_t vaddr_start, uint64_t vaddr_end, uint64_t offset, struct address_space_t *address_space) {
  for (uint64_t va = vaddr_start; va < vaddr_end; va += DEFAULT_PAGE_SIZE) {
    uint64_t page_index = (va - vaddr_start) / DEFAULT_PAGE_SIZE;
    uint64_t file_page_offset = (page_index * DEFAULT_PAGE_SIZE + offset) & ~(DEFAULT_PAGE_SIZE - 1);

    list_for_each(&address_space->page_cache_list, pos) {
      struct page_cache_entry_t *entry = container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
      if (entry->offset == file_page_offset) {
        if (entry->refcount == 0)
          break;
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
