#ifndef VFS_H
#define VFS_H

#include "types.h"
#include "kernel/filesystem/mode.h"
#include "lib/pool_allocator.h"
#include "lib/list.h"

#define MAX_DENTRIES 256
/* File open flags */
#define O_RDONLY    0x0000  /* Open for reading only */
#define O_WRONLY    0x0001  /* Open for writing only */
#define O_RDWR      0x0002  /* Open for reading and writing */
#define O_ACCMODE   0x0003  /* Mask for file access modes */

/* File creation and status flags */
#define O_CREAT     0x0040  /* Create file if it doesn't exist */
#define O_EXCL      0x0080  /* Exclusive use (with O_CREAT) */
#define O_TRUNC     0x0200  /* Truncate file to zero length */
#define O_APPEND    0x0400  /* Append mode */

/* File descriptor flags */
#define O_CLOEXEC   0x0800  /* Close on exec */

extern struct mount_t *base_mount;

struct vnode_t;
struct dentry_t;
struct address_space_t;
struct file_t;
struct superblock_t;

struct file_ops_t {
  int64_t (*read)(struct file_t *file, uint64_t offset, void *buffer, uint64_t size);
};

struct vnode_ops_t {
  //int64_t (*read)(struct vnode_t *vnode, void *buffer, uint64_t offset, uint64_t size);
  int64_t (*lookup)(const char *name, struct vnode_t *parent_dir, struct dentry_t **out);
};

struct address_space_ops_t {
  int64_t (*fill_page)(struct vnode_t *vnode,  size_t offset, void **phys_page);
};

struct superblock_ops_t {
  struct vnode_t *(*alloc_vnode)(struct superblock_t superblock);
};

struct file_t {
  struct vnode_t *vnode;
  struct file_t_ops *file_ops;
  size_t offset;
  size_t refcount;
  int flags;  /* O_RDONLY, O_WRONLY, O_RDWR, etc. */
};

struct dentry_t {
  char name[256];
  struct vnode_t *vnode;

  struct dentry_t *parent;
  struct list_node sibling_dentry;
};

struct page_cache_entry_t {
  size_t offset;
  void *physical_page;
  size_t refcount;
  bool dirty;
  struct list_node sibling_page_cache_entry;
};

struct address_space_t { // struct which holds all pages cached in memory of file contents
  size_t num_pages_used;
  struct list_node page_cache_list;
  struct address_space_ops_t *address_space_ops;
};

struct vnode_t {
  uint64_t size;
  uint32_t id;
  uint32_t refcount;
  uint32_t owner_uid;
  uint32_t owner_gid;
  mode_t permission_mode;
  struct superblock_t *superblock;
  struct list_node children_dentries; // Sentinel node into the dentry list of children, null if not a director
  struct vnode_ops_t *vnode_ops;
  struct address_space_t *address_space;
  void *fs_private_vnode;
};

struct superblock_t {
  struct vnode_ops_t vnode_ops;
  struct address_space_ops_t address_space_ops;
  struct superblock_ops_t superblock_ops;
  struct file_ops_t file_ops;
  struct vnode_t *root_vnode;
  struct dentry_t *root_dentry;
  void *private_data;
  uint64_t block_size;
  char device[32];
};

struct mount_t {
  char root_path[256];
  struct superblock_t *superblock;
  struct mount_t *next_mount;
};

DEFINE_POOL(mount_t, struct mount_t)
DEFINE_POOL(superblock_t, struct superblock_t)
DEFINE_POOL(vnode_t, struct vnode_t)
DEFINE_POOL(dentry_t, struct dentry_t)
DEFINE_POOL(page_cache_entry_t, struct page_cache_entry_t)
DEFINE_POOL(address_space_t, struct address_space_t)
DEFINE_POOL(address_space_ops_t, struct address_space_ops_t)

void init_vnode(struct vnode_t *vnode, struct superblock_t *sb, uint32_t id);
void vfs_print_vnode(struct vnode_t *vnode);
void vfs_print_dentry(struct dentry_t *dentry);

void vfs_init();
void vfs_mount(char *path, struct superblock_t *superblock);

int32_t vfs_resolve_path(const char *path, struct dentry_t **out);
int32_t vfs_lookup(const char *name, struct vnode_t *parent_dir, struct dentry_t **out);
void *vfs_get_page(struct vnode_t *vnode, size_t offset);
int32_t vfs_vnode_read(struct vnode_t *vnode, void *buf, size_t size, size_t offset);
int64_t vfs_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size);
int64_t vfs_open(const char *path, int flags, struct file_t **file);
struct file_t *vfs_init_file(struct vnode_t *vnode, int flags);

#endif
