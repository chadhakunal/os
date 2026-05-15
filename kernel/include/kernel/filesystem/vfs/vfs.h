#ifndef VFS_H
#define VFS_H

#include "types.h"
#include "kernel/filesystem/mode.h"
#include "lib/pool_allocator.h"
#include "lib/list.h"

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

#define MAX_DENTRIES      256
#define MAX_PATH_NAME_LEN 256

/* open(2) flags */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_CLOEXEC   0x0800

/* vfs_get_page flags */
#define VFS_PAGE_NOREF  0x0000
#define VFS_PAGE_REF    0x0001

/* superblock_t flags */
#define SB_NODENTRY_CACHE  0x0001  /* don't cache vnode_ops->lookup results in children_dentries */

/* -------------------------------------------------------------------------
 * Forward declarations
 * ---------------------------------------------------------------------- */

struct vnode_t;
struct dentry_t;
struct address_space_t;
struct file_t;
struct superblock_t;
struct files_table_t;

extern struct mount_t *base_mount;

/* -------------------------------------------------------------------------
 * Operations tables (filesystem-provided vtables)
 * ---------------------------------------------------------------------- */

struct file_ops_t {
  int64_t (*read)(struct file_t *file, uint64_t offset, void *buffer, uint64_t size);
  int64_t (*write)(struct file_t *file, uint64_t offset, void *buffer, uint64_t size);
  int     (*ioctl)(struct file_t *file, unsigned long request, void *arg);
};

struct vnode_ops_t {
  int64_t (*lookup) (const char *name, struct vnode_t *parent_dir, struct dentry_t **out);
  int64_t (*readdir)(struct vnode_t *dir, uint32_t index, struct dentry_t **out);
  int64_t (*create) (const char *name, struct vnode_t *parent_dir, struct dentry_t **out);
  int64_t (*mkdir)  (const char *name, struct vnode_t *parent_dir, struct dentry_t **out);
  int64_t (*unlink) (const char *name, struct vnode_t *parent_dir);
  int64_t (*rmdir)  (const char *name, struct vnode_t *parent_dir);
};

struct address_space_ops_t {
  int64_t (*fill_page) (struct vnode_t *vnode, size_t offset, void **phys_page);
  int64_t (*write_page)(struct vnode_t *vnode, size_t offset, void *phys_page);
};

struct superblock_ops_t {
  struct vnode_t *(*alloc_vnode)(struct superblock_t superblock);
};

/* -------------------------------------------------------------------------
 * Core data structures
 * ---------------------------------------------------------------------- */

struct file_t {
  struct vnode_t    *vnode;
  struct file_ops_t *file_ops;
  size_t             offset;
  size_t             refcount;
  int                flags;
};

struct dentry_t {
  char              name[256];
  struct vnode_t   *vnode;
  struct dentry_t  *parent;
  struct list_node  sibling_dentry;
  uint32_t          readdir_ino; /* inode number for ephemeral readdir dentries (vnode == NULL) */
};

struct page_cache_entry_t {
  size_t           offset;
  void            *physical_page;
  size_t           refcount;
  bool             dirty;
  struct list_node sibling_page_cache_entry;
};

struct address_space_t {
  size_t                      num_pages_used;
  struct list_node            page_cache_list;
  struct address_space_ops_t *address_space_ops;
};

struct vnode_t {
  uint64_t               size;
  uint32_t               id;
  uint32_t               refcount;
  uint32_t               owner_uid;
  uint32_t               owner_gid;
  mode_t                 permission_mode;
  struct vnode_t        *mounted_vnode;
  struct superblock_t   *superblock;
  struct list_node       children_dentries;
  struct vnode_ops_t    *vnode_ops;
  struct file_ops_t     *file_ops;
  struct address_space_t *address_space;
  void                  *fs_private_vnode;
};

struct superblock_t {
  struct vnode_ops_t         vnode_ops;
  struct address_space_ops_t address_space_ops;
  struct superblock_ops_t    superblock_ops;
  struct file_ops_t          file_ops;
  struct vnode_t            *root_vnode;
  struct dentry_t           *root_dentry;
  void                      *private_data;
  uint64_t                   block_size;
  uint32_t                   flags;
  char                       device[32];
};

struct mount_t {
  char              root_path[256];
  struct superblock_t *superblock;
  struct list_node  sibling_mount;
};

/* -------------------------------------------------------------------------
 * Pool allocators
 * ---------------------------------------------------------------------- */

DEFINE_POOL(mount_t, struct mount_t)
DEFINE_POOL(superblock_t, struct superblock_t)
DEFINE_POOL(vnode_t, struct vnode_t)
DEFINE_POOL(dentry_t, struct dentry_t)
DEFINE_POOL(page_cache_entry_t, struct page_cache_entry_t)
DEFINE_POOL(address_space_t, struct address_space_t)
DEFINE_POOL(address_space_ops_t, struct address_space_ops_t)
DEFINE_POOL(file_t, struct file_t)

/* -------------------------------------------------------------------------
 * VFS functions
 * ---------------------------------------------------------------------- */

/* Init / mount */
void    vfs_init(void);
int32_t vfs_mount(char *path, struct superblock_t *superblock);

/* Vnode / dentry helpers */
void    vfs_init_vnode(struct vnode_t *vnode, struct superblock_t *sb, uint32_t id);
void    vfs_print_vnode(struct vnode_t *vnode);
void    vfs_print_dentry(struct dentry_t *dentry);
int64_t vfs_dentry_get_path(struct dentry_t *dentry, char *buf, size_t size);

/* Path resolution */
int32_t vfs_resolve_path(const char *path, struct dentry_t **out);
int32_t vfs_lookup(const char *name, struct dentry_t *parent_dentry, struct dentry_t **out);

/* Directory operations */
int64_t vfs_readdir(struct vnode_t *dir, uint32_t index, struct dentry_t **out);
int64_t vfs_create(const char *name, struct vnode_t *parent_dir, struct dentry_t **out);
int64_t vfs_mkdir(const char *name, struct vnode_t *parent_dir, struct dentry_t **out);
int64_t vfs_unlink(const char *name, struct vnode_t *parent_dir);
int64_t vfs_rmdir(const char *name, struct vnode_t *parent_dir);

/* File operations */
int64_t         vfs_open(const char *path, int flags, struct file_t **file);
struct file_t  *vfs_init_file(struct vnode_t *vnode, int flags);
int64_t         vfs_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size);
int64_t         vfs_write(struct file_t *file, uint64_t offset, void *buffer, uint64_t size);
int64_t         vfs_file_close(struct files_table_t *file_table, int fd);
int64_t         vfs_fsync(struct files_table_t *file_table, int fd);
int64_t         vfs_dup2(struct files_table_t *file_table, int oldfd, int newfd);
int64_t         vfs_file_lseek(struct files_table_t *file_table, int fd, int64_t offset, int whence);

/* Page cache — internal VFS use only, not for syscalls */
void   *vfs_get_page(struct vnode_t *vnode, size_t offset, int flags);
int32_t vfs_vnode_read(struct vnode_t *vnode, void *buf, size_t size, size_t offset);
int32_t vfs_vnode_write(struct vnode_t *vnode, const void *buf, size_t size, size_t offset);
void    vfs_address_space_inc_ref(uint64_t vaddr_start, uint64_t vaddr_end, uint64_t offset, struct address_space_t *address_space);
void    vfs_address_space_drop_ref(uint64_t vaddr_start, uint64_t vaddr_end, uint64_t offset, struct address_space_t *address_space);
void    vfs_invalidate_page_cache(struct vnode_t *vnode);

#endif
