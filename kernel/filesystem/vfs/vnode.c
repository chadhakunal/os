#include "kernel/filesystem/vfs/vfs.h"
#include "lib/printk/printk.h"
#include "kernel/filesystem/mode.h"
#include "lib/string.h"
#include "virtual_memory_init.h"
#include "panic.h"
#include "kernel/memory/page_allocator.h"

void init_vnode(struct vnode_t *vnode, struct superblock_t *sb, uint32_t id, mode_t permission_mode, uint64_t size) {
  vnode->superblock = sb;
  vnode->ops = &sb->vnode_ops;
  vnode->id = id;
  vnode->refcount = 1;
  vnode->owner_uid = 0;
  vnode->owner_gid = 0;
  vnode->permission_mode = permission_mode;
  vnode->size = size;
  vnode->children_dentries.next = &vnode->children_dentries;
  vnode->children_dentries.prev = &vnode->children_dentries;
  vnode->address_space.num_pages_used = 0;
  vnode->address_space.page_cache_list.next = &vnode->address_space.page_cache_list;
  vnode->address_space.page_cache_list.prev = &vnode->address_space.page_cache_list;
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
    void *page_phys = vfs_get_page(vnode, page_offset);
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
