#ifndef SBFS_H
#define SBFS_H

#include "kernel/filesystem/vfs/vfs.h"
#include "lib/pool_allocator.h"
#include "types.h"

#define SBFS_MAGIC        0x53464253
#define SBFS_DIRECT_BLOCKS 12
#define SBFS_DIRENT_NAME_LEN 28

#define SBFS_INODE_FREE    0
#define SBFS_INODE_FILE    1
#define SBFS_INODE_DIR     2
#define SBFS_INODE_SYMLINK 3

/* On-disk superblock — exactly as written by mkfs, lives at block 0. */
typedef struct sbfs_disk_superblock {
  uint32_t magic;
  uint32_t block_size;
  uint32_t total_blocks;
  uint32_t inode_count;
  uint32_t inode_size;
  uint32_t inode_bitmap_start;
  uint32_t inode_bitmap_num_blocks;
  uint32_t block_bitmap_start;
  uint32_t block_bitmap_num_blocks;
  uint32_t inode_table_start;
  uint32_t inode_table_num_blocks;
  uint32_t data_start;
  uint32_t root_inode;
  uint8_t  reserved[512 - 13 * 4];
} sbfs_disk_superblock_t;

/* On-disk inode — 64 bytes. */
typedef struct sbfs_inode {
  uint16_t type;
  uint16_t nlinks;
  uint32_t size;
  uint32_t direct_blocks[SBFS_DIRECT_BLOCKS];
  uint32_t indirect_block;
  uint32_t reserved;
} sbfs_inode_t;

/* On-disk directory entry — 32 bytes. */
typedef struct sbfs_dirent {
  uint32_t inode_num;
  char     name[SBFS_DIRENT_NAME_LEN];
} sbfs_dirent_t;

#define SBFS_MAX_BITMAP_PAGES      8
#define SBFS_MAX_INODE_TABLE_PAGES 16

/* In-memory superblock — layout fields + loaded metadata pages. */
struct sbfs_superblock_t {
  uint32_t block_size;
  uint32_t total_blocks;
  uint32_t inode_count;
  uint32_t inode_size;
  uint32_t inode_bitmap_start;
  uint32_t inode_bitmap_num_blocks;
  uint32_t block_bitmap_start;
  uint32_t block_bitmap_num_blocks;
  uint32_t inode_table_start;
  uint32_t inode_table_num_blocks;
  uint32_t data_start;
  uint32_t last_vnode_id;

  /* Physical pages holding each metadata region, loaded at mount time. */
  void *inode_bitmap_pages[SBFS_MAX_BITMAP_PAGES];
  void *block_bitmap_pages[SBFS_MAX_BITMAP_PAGES];
  void *inode_table_pages[SBFS_MAX_INODE_TABLE_PAGES];
};

/* In-memory per-vnode private data.
 * Must be >= sizeof(void*) so the pool allocator's free-list next pointer
 * (8 bytes on 64-bit) fits inside a free slot without overflowing. */
struct sbfs_vnode_t {
  uint32_t inode_num;
  uint32_t _pad;
};

DEFINE_POOL(sbfs_superblock_t, struct sbfs_superblock_t)
DEFINE_POOL(sbfs_vnode_t, struct sbfs_vnode_t)

/* Mount */
struct superblock_t *sbfs_mount(void);

/* vnode_ops */
int64_t sbfs_lookup (const char *name, struct vnode_t *parent_dir, struct dentry_t **out);
int64_t sbfs_readdir(struct vnode_t *dir, uint32_t index, struct dentry_t **out);
int64_t sbfs_create (const char *name, struct vnode_t *parent_dir, struct dentry_t **out);
int64_t sbfs_mkdir  (const char *name, struct vnode_t *parent_dir, struct dentry_t **out);
int64_t sbfs_unlink (const char *name, struct vnode_t *parent_dir);
int64_t sbfs_rmdir  (const char *name, struct vnode_t *parent_dir);
int64_t sbfs_rename (const char *old_name, struct vnode_t *old_parent,
                     const char *new_name, struct vnode_t *new_parent);
int64_t sbfs_link   (const char *old_name, struct vnode_t *old_parent,
                     const char *new_name, struct vnode_t *new_parent);
int64_t sbfs_symlink(const char *target, const char *name, struct vnode_t *parent_dir,
                     struct dentry_t **out);
int64_t sbfs_readlink(struct vnode_t *vnode, char *buf, size_t size);

/* superblock_ops */
int64_t sbfs_statfs (struct superblock_t *sb, struct vfs_statfs *buf);

/* address_space_ops */
int64_t sbfs_fill_page (struct vnode_t *vnode, size_t offset, void **phys_page);
int64_t sbfs_write_page(struct vnode_t *vnode, size_t offset, void *phys_page);

#endif
