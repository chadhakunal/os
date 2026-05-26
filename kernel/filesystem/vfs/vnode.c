#define DEBUG 0
#include "kernel/filesystem/vfs/vfs.h"
#include "lib/printk/printk.h"
#include "kernel/filesystem/mode.h"
#include "lib/string.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/panic.h"
#include "kernel/memory/page_allocator.h"
#include "errno.h"

void vfs_init_vnode(struct vnode_t *vnode, struct superblock_t *sb, uint32_t id) {
  vnode->superblock = sb;
  vnode->vnode_ops = &sb->vnode_ops;
  vnode->file_ops = &sb->file_ops;
  vnode->id = id;
  vnode->refcount = 0;
  vnode->owner_uid = 0;
  vnode->owner_gid = 0;
  vnode->permission_mode = 0;
  vnode->size = 0;
  vnode->mtime = 0;
  vnode->mounted_vnode = NULL;
  vnode->address_space = NULL;
  vnode->children_dentries.next = &vnode->children_dentries;
  vnode->children_dentries.prev = &vnode->children_dentries;
  vnode->fs_private_vnode = NULL;
}

void vfs_print_vnode(struct vnode_t *vnode) {
  if (vnode == NULL) {
    debugk("[vnode: NULL]\n");
    return;
  }
  debugk("[vnode id=%d, size=%lld, refcount=%d, uid=%d, gid=%d, mode=0x%x%s, fs_private=%p]\n",
         vnode->id,
         vnode->size,
         vnode->refcount,
         vnode->owner_uid,
         vnode->owner_gid,
         vnode->permission_mode,
         IS_DIR(vnode->permission_mode) ? " (DIR)" : "",
         vnode->fs_private_vnode);
}

int32_t vfs_vnode_read(struct vnode_t *vnode, void *buf, size_t size, size_t offset) {
  if (vnode == NULL || buf == NULL) {
    panic("vfs_vnode_read: NULL parameter\n");
  }

  debugk("[vfs_vnode_read] vnode=%p vnode->size=%llu offset=%zu req=%zu\n",
         vnode, (unsigned long long)vnode->size, offset, size);

  // Handle EOF
  if (offset >= vnode->size) {
    debugk("[vfs_vnode_read] returning 0 (EOF)\n");
    return 0;  // 0 bytes read = EOF
  }

  // Clamp size to file size
  if (offset + size > vnode->size) {
    size = vnode->size - offset;
  }

  size_t total_copied = 0;
  uint8_t *dest = (uint8_t *)buf;

  while (size > 0) {
    // Calculate page-aligned offset and offset within page
    size_t page_offset = offset & ~(DEFAULT_PAGE_SIZE - 1);
    size_t offset_in_page = offset - page_offset;
    size_t bytes_in_page = DEFAULT_PAGE_SIZE - offset_in_page;
    size_t copy_size = (size < bytes_in_page) ? size : bytes_in_page;

    // Get page from cache
    void *page_phys = vfs_get_page(vnode, page_offset, VFS_PAGE_NOREF);
    if (!page_phys) {
      return -1;  // Error reading page
    }

    // Copy from page to buffer
    void *page_virt = PHYS_TO_VIRT(page_phys);
    memcpy(dest, (uint8_t *)page_virt + offset_in_page, copy_size);

    // Advance
    dest += copy_size;
    offset += copy_size;
    size -= copy_size;
    total_copied += copy_size;
  }

  return total_copied;
}

#define DIR_W_CHECK(dir) \
  if ((dir) == NULL || !IS_DIR((dir)->permission_mode)) return -ENOTDIR; \
  if (!((dir)->permission_mode & PERM_WUSR)) return -EACCES;

int64_t vfs_create(const char *name, struct vnode_t *parent_dir,
                   struct dentry_t **out, uint32_t mode) {
  debugk("[vfs_create] name=%s parent_mode=0x%x mode=0%o\n",
         name, parent_dir ? parent_dir->permission_mode : 0, mode);
  DIR_W_CHECK(parent_dir);
  if (parent_dir->vnode_ops == NULL || parent_dir->vnode_ops->create == NULL)
    return -1;
  return parent_dir->vnode_ops->create(name, parent_dir, out, mode);
}

int64_t vfs_mkdir(const char *name, struct vnode_t *parent_dir, struct dentry_t **out) {
  debugk("[vfs_mkdir] name=%s parent_mode=0x%x\n", name, parent_dir ? parent_dir->permission_mode : 0);
  DIR_W_CHECK(parent_dir);
  if (parent_dir->vnode_ops == NULL || parent_dir->vnode_ops->mkdir == NULL)
    return -1;
  return parent_dir->vnode_ops->mkdir(name, parent_dir, out);
}

int64_t vfs_unlink(const char *name, struct vnode_t *parent_dir) {
  DIR_W_CHECK(parent_dir);
  if (parent_dir->vnode_ops == NULL || parent_dir->vnode_ops->unlink == NULL)
    return -1;
  return parent_dir->vnode_ops->unlink(name, parent_dir);
}

int64_t vfs_rmdir(const char *name, struct vnode_t *parent_dir) {
  DIR_W_CHECK(parent_dir);
  if (parent_dir->vnode_ops == NULL || parent_dir->vnode_ops->rmdir == NULL)
    return -1;
  return parent_dir->vnode_ops->rmdir(name, parent_dir);
}

/* Remove a cached dentry by name from a vnode's children_dentries list. */
void vfs_dentry_evict(struct vnode_t *parent_vnode, const char *name) {
  struct list_node *pos = parent_vnode->children_dentries.next;
  while (pos != &parent_vnode->children_dentries) {
    struct dentry_t *d = container_of(pos, struct dentry_t, sibling_dentry);
    pos = pos->next;
    if (strncmp(d->name, name) == 0) {
      list_remove(&d->sibling_dentry);
      dentry_t_free(d);
      return;
    }
  }
}

int64_t vfs_rename(const char *old_name, struct vnode_t *old_parent,
                   const char *new_name, struct vnode_t *new_parent) {
  if (old_parent == NULL || !IS_DIR(old_parent->permission_mode))
    return -ENOTDIR;
  if (new_parent == NULL || !IS_DIR(new_parent->permission_mode))
    return -ENOTDIR;
  if (!(old_parent->permission_mode & PERM_WUSR)) return -EACCES;
  if (!(new_parent->permission_mode & PERM_WUSR)) return -EACCES;
  if (old_parent->superblock != new_parent->superblock)
    return -EXDEV;
  if (old_parent->vnode_ops == NULL || old_parent->vnode_ops->rename == NULL)
    return -1;

  int64_t ret = old_parent->vnode_ops->rename(old_name, old_parent, new_name, new_parent);
  if (ret == 0) {
    vfs_dentry_evict(old_parent, old_name);
    vfs_dentry_evict(new_parent, new_name);
  }
  return ret;
}

int64_t vfs_symlink(const char *target, const char *name,
                    struct vnode_t *parent_dir, struct dentry_t **out) {
  if (parent_dir == NULL || !IS_DIR(parent_dir->permission_mode))
    return -ENOTDIR;
  if (!(parent_dir->permission_mode & PERM_WUSR)) return -EACCES;
  if (parent_dir->vnode_ops == NULL || parent_dir->vnode_ops->symlink == NULL)
    return -1;
  return parent_dir->vnode_ops->symlink(target, name, parent_dir, out);
}

int64_t vfs_readlink(struct vnode_t *vnode, char *buf, size_t size) {
  if (vnode == NULL || !IS_LNK(vnode->permission_mode))
    return -EINVAL;
  if (vnode->vnode_ops == NULL || vnode->vnode_ops->readlink == NULL)
    return -1;
  return vnode->vnode_ops->readlink(vnode, buf, size);
}

int64_t vfs_link(const char *old_name, struct vnode_t *old_parent,
                 const char *new_name, struct vnode_t *new_parent) {
  if (old_parent == NULL || !IS_DIR(old_parent->permission_mode))
    return -ENOTDIR;
  if (new_parent == NULL || !IS_DIR(new_parent->permission_mode))
    return -ENOTDIR;
  if (!(new_parent->permission_mode & PERM_WUSR)) return -EACCES;
  if (old_parent->superblock != new_parent->superblock)
    return -EXDEV;
  if (old_parent->vnode_ops == NULL || old_parent->vnode_ops->link == NULL)
    return -1;
  return old_parent->vnode_ops->link(old_name, old_parent, new_name, new_parent);
}

int64_t vfs_stat(struct vnode_t *vnode, struct vfs_stat *buf) {
  if (vnode == NULL || buf == NULL)
    return -1;
  buf->st_ino   = vnode->id;
  buf->st_mode  = vnode->permission_mode;
  buf->st_nlink = 1;
  buf->st_size  = vnode->size;
  buf->st_mtime = vnode->mtime;
  return 0;
}

int64_t vfs_chmod(struct vnode_t *vnode, uint32_t mode) {
  if (vnode == NULL)
    return -1;
  if (vnode->vnode_ops == NULL || vnode->vnode_ops->chmod == NULL) {
    /* For read-only or virtual filesystems, just update the in-memory vnode. */
    uint32_t type_bits = vnode->permission_mode & S_IFMT;
    vnode->permission_mode = type_bits | (mode & ~(uint32_t)S_IFMT);
    return 0;
  }
  return vnode->vnode_ops->chmod(vnode, mode);
}

int64_t vfs_truncate(struct vnode_t *vnode, uint64_t new_size) {
  if (vnode == NULL)
    return -EINVAL;
  if (IS_DIR(vnode->permission_mode))
    return -EISDIR;
  if (!(vnode->permission_mode & PERM_WUSR))
    return -EACCES;
  if (vnode->vnode_ops == NULL || vnode->vnode_ops->truncate == NULL)
    return -EINVAL;
  /* Push cached writes to disk before shrinking the file or dropping cache. */
  vfs_flush_dirty_pages(vnode);
  int64_t ret = vnode->vnode_ops->truncate(vnode, new_size);
  if (ret < 0)
    return ret;
  vfs_invalidate_page_cache(vnode);
  return 0;
}

int64_t vfs_statfs(struct vnode_t *vnode, struct vfs_statfs *buf) {
  if (vnode == NULL || buf == NULL)
    return -EINVAL;
  struct superblock_t *sb = vnode->superblock;
  if (sb == NULL || sb->superblock_ops.statfs == NULL)
    return -ENOSYS;
  return sb->superblock_ops.statfs(sb, buf);
}

int32_t vfs_vnode_write(struct vnode_t *vnode, const void *buf, size_t size, size_t offset) {
  if (vnode == NULL || buf == NULL)
    panic("vfs_vnode_write: NULL parameter\n");
  if (vnode->address_space == NULL ||
      vnode->address_space->address_space_ops == NULL ||
      vnode->address_space->address_space_ops->write_page == NULL)
    panic("vfs_vnode_write: no write_page op\n");

  size_t total_written = 0;
  const uint8_t *src = (const uint8_t *)buf;

  while (size > 0) {
    size_t page_offset    = offset & ~(DEFAULT_PAGE_SIZE - 1);
    size_t offset_in_page = offset - page_offset;
    size_t bytes_in_page  = DEFAULT_PAGE_SIZE - offset_in_page;
    size_t copy_size      = (size < bytes_in_page) ? size : bytes_in_page;

    /* Get (or populate) the page in the cache so we can do a read-modify-write. */
    void *page_phys = vfs_get_page(vnode, page_offset, VFS_PAGE_NOREF);
    if (page_phys == NULL)
      return total_written > 0 ? (int32_t)total_written : -1;

    memcpy((uint8_t *)PHYS_TO_VIRT(page_phys) + offset_in_page, src, copy_size);

    /* Mark the page dirty in the cache — it will be flushed on close. */
    list_for_each(&vnode->address_space->page_cache_list, pos) {
      struct page_cache_entry_t *entry =
        container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
      if (entry->offset == page_offset) {
        entry->dirty = true;
        break;
      }
    }

    src           += copy_size;
    offset        += copy_size;
    size          -= copy_size;
    total_written += copy_size;
  }

  /* Update vnode size if the write extended the file. */
  if (offset > vnode->size)
    vnode->size = offset;

  return (int32_t)total_written;
}

int64_t vfs_readdir(struct vnode_t *dir, uint32_t index, struct dentry_t **out) {
  if (dir == NULL || !IS_DIR(dir->permission_mode))
    return -1;

  /*
   * Phase 1: serve entries from the in-memory children_dentries list.
   * These are mount-point stubs (bin, etc, dev, proc, …) registered by
   * vfs_init.  They must appear in readdir even when the underlying
   * filesystem knows nothing about them.
   */
  uint32_t mem_count = 0;
  list_for_each(&dir->children_dentries, pos) {
    if (mem_count == index) {
      *out = container_of(pos, struct dentry_t, sibling_dentry);
      return 0;
    }
    mem_count++;
  }

  /*
   * Phase 2: delegate to the filesystem's readdir for on-disk entries,
   * skipping any name that is already covered by a children_dentries stub
   * so that mount points are never reported twice.
   */
  if (dir->vnode_ops == NULL || dir->vnode_ops->readdir == NULL)
    return -1;

  uint32_t fs_index = index - mem_count;
  uint32_t fs_scanned = 0;

  while (1) {
    struct dentry_t *candidate = NULL;
    int64_t ret = dir->vnode_ops->readdir(dir, fs_scanned, &candidate);
    if (ret < 0 || candidate == NULL)
      return -1;

    /* Skip if this name is already exposed via children_dentries. */
    bool shadowed = false;
    list_for_each(&dir->children_dentries, pos) {
      struct dentry_t *stub = container_of(pos, struct dentry_t, sibling_dentry);
      if (strncmp(stub->name, candidate->name) == 0) {
        shadowed = true;
        break;
      }
    }

    if (!shadowed) {
      if (fs_index == 0) {
        *out = candidate;
        return 0;
      }
      fs_index--;
    } else {
      /* Free the ephemeral dentry if it was freshly allocated by the FS
       * (vnode == NULL is the convention used by sbfs_readdir). */
      if (candidate->vnode == NULL)
        dentry_t_free(candidate);
    }

    fs_scanned++;
  }
}

int32_t vfs_lookup(const char *name, struct dentry_t *parent_dentry, struct dentry_t **out) {
  if (parent_dentry == NULL || parent_dentry->vnode == NULL) {
    *out = NULL;
    return -ENOENT;
  }

  struct vnode_t *parent_dir = parent_dentry->vnode->mounted_vnode != NULL
                                  ? parent_dentry->vnode->mounted_vnode
                                  : parent_dentry->vnode;

  if (!IS_DIR(parent_dir->permission_mode)) {
    *out = NULL;
    return -ENOTDIR;
  }

  list_for_each(&parent_dir->children_dentries, pos) {
    struct dentry_t *dentry = container_of(pos, struct dentry_t, sibling_dentry);
    if (strncmp(dentry->name, name) == 0) {
      *out = dentry;
      (*out)->parent = parent_dentry;
      return 0;
    }
  }
  if (parent_dir->vnode_ops->lookup == NULL) {
    *out = NULL;
    return -ENOENT;
  }

  int32_t ret = parent_dir->vnode_ops->lookup(name, parent_dir, out);
  if (ret != 0 || *out == NULL) {
    *out = NULL;
    return -ENOENT;
  }

  (*out)->parent = parent_dentry;
  if (!(parent_dir->superblock->flags & SB_NODENTRY_CACHE))
    list_append(&parent_dir->children_dentries, &(*out)->sibling_dentry);
  return 0;
}
