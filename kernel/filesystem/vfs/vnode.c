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
  vnode->refcount = 1;
  vnode->owner_uid = 0;
  vnode->owner_gid = 0;
  vnode->permission_mode = 0;
  vnode->size = 0;
  vnode->children_dentries.next = &vnode->children_dentries;
  vnode->children_dentries.prev = &vnode->children_dentries;
  vnode->fs_private_vnode = NULL;
}

void vfs_print_vnode(struct vnode_t *vnode) {
  if (vnode == NULL) {
    printk("[vnode: NULL]\n");
    return;
  }
  printk("[vnode id=%d, size=%lld, refcount=%d, uid=%d, gid=%d, mode=0x%x%s, fs_private=%p]\n",
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

  // Handle EOF
  if (offset >= vnode->size) {
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

int64_t vfs_create(const char *name, struct vnode_t *parent_dir, struct dentry_t **out) {
  if (parent_dir == NULL || !IS_DIR(parent_dir->permission_mode))
    return -ENOTDIR;
  if (parent_dir->vnode_ops == NULL || parent_dir->vnode_ops->create == NULL)
    return -1;
  return parent_dir->vnode_ops->create(name, parent_dir, out);
}

int64_t vfs_mkdir(const char *name, struct vnode_t *parent_dir, struct dentry_t **out) {
  if (parent_dir == NULL || !IS_DIR(parent_dir->permission_mode))
    return -ENOTDIR;
  if (parent_dir->vnode_ops == NULL || parent_dir->vnode_ops->mkdir == NULL)
    return -1;
  return parent_dir->vnode_ops->mkdir(name, parent_dir, out);
}

int64_t vfs_unlink(const char *name, struct vnode_t *parent_dir) {
  if (parent_dir == NULL || !IS_DIR(parent_dir->permission_mode))
    return -ENOTDIR;
  if (parent_dir->vnode_ops == NULL || parent_dir->vnode_ops->unlink == NULL)
    return -1;
  return parent_dir->vnode_ops->unlink(name, parent_dir);
}

int64_t vfs_rmdir(const char *name, struct vnode_t *parent_dir) {
  if (parent_dir == NULL || !IS_DIR(parent_dir->permission_mode))
    return -ENOTDIR;
  if (parent_dir->vnode_ops == NULL || parent_dir->vnode_ops->rmdir == NULL)
    return -1;
  return parent_dir->vnode_ops->rmdir(name, parent_dir);
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

    /* Flush the modified page back to the filesystem. */
    int ret = vnode->address_space->address_space_ops->write_page(vnode, page_offset, page_phys);
    if (ret < 0)
      return total_written > 0 ? (int32_t)total_written : ret;

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
  if (dir->vnode_ops == NULL || dir->vnode_ops->readdir == NULL)
    return -1;
  return dir->vnode_ops->readdir(dir, index, out);
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
  list_append(&parent_dir->children_dentries, &(*out)->sibling_dentry);
  return 0;
}
