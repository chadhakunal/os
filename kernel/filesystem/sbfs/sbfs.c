#include "kernel/filesystem/sbfs/sbfs.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/drivers/virtio-blk.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/filesystem/mode.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "errno.h"
#include "types.h"

#define SECTORS_PER_PAGE (DEFAULT_PAGE_SIZE / SECTOR_BLOCK_SIZE)

static struct vnode_t *sbfs_alloc_vnode(struct superblock_t *superblock, uint32_t inode_num);
static int64_t sbfs_truncate(struct vnode_t *vnode, uint64_t new_size);

/* -------------------------------------------------------------------------
 * Low-level disk I/O — always a full page at a time
 * ---------------------------------------------------------------------- */

/* Read one page from `first_sector` into physical page `phys`. */
static void sbfs_read_page(uint64_t first_sector, void *phys) {
  virtio_blk_read(first_sector, PHYS_TO_VIRT(phys), SECTORS_PER_PAGE);
}

/* Write one page from physical page `phys` to `first_sector`. */
static void sbfs_write_disk_page(uint64_t first_sector, void *phys) {
  virtio_blk_write(first_sector, PHYS_TO_VIRT(phys), SECTORS_PER_PAGE);
}

/* Convert a disk block number to its first sector number. */
static uint64_t sbfs_block_to_sector(struct sbfs_superblock_t *sb, uint32_t block) {
  return (uint64_t)block * (sb->block_size / SECTOR_BLOCK_SIZE);
}

/* -------------------------------------------------------------------------
 * Metadata accessors and flush helpers
 * ---------------------------------------------------------------------- */

static uint32_t sbfs_blocks_per_page(struct sbfs_superblock_t *sb) {
  return DEFAULT_PAGE_SIZE / sb->block_size;
}

/* Pointer to the byte in the inode bitmap that contains bit `inode_num`. */
static uint8_t *sbfs_inode_bitmap_byte(struct sbfs_superblock_t *sb, uint32_t inode_num) {
  uint32_t byte_off = inode_num / 8;
  uint32_t page_idx = byte_off / DEFAULT_PAGE_SIZE;
  uint32_t page_off = byte_off % DEFAULT_PAGE_SIZE;
  return (uint8_t *)PHYS_TO_VIRT(sb->inode_bitmap_pages[page_idx]) + page_off;
}

/* Pointer to the byte in the block bitmap that contains bit `block_num`. */
static uint8_t *sbfs_block_bitmap_byte(struct sbfs_superblock_t *sb, uint32_t block_num) {
  uint32_t byte_off = block_num / 8;
  uint32_t page_idx = byte_off / DEFAULT_PAGE_SIZE;
  uint32_t page_off = byte_off % DEFAULT_PAGE_SIZE;
  return (uint8_t *)PHYS_TO_VIRT(sb->block_bitmap_pages[page_idx]) + page_off;
}

/* Pointer to the on-disk inode struct for `inode_num`. */
static sbfs_inode_t *sbfs_get_inode(struct sbfs_superblock_t *sb, uint32_t inode_num) {
  uint32_t inodes_per_block = sb->block_size / sb->inode_size;
  uint32_t block_off        = inode_num / inodes_per_block;
  uint32_t bpp              = sbfs_blocks_per_page(sb);
  uint32_t page_idx         = block_off / bpp;
  uint32_t page_off         = block_off % bpp;
  uint8_t *base = (uint8_t *)PHYS_TO_VIRT(sb->inode_table_pages[page_idx])
                  + page_off * sb->block_size;
  return (sbfs_inode_t *)(base + (inode_num % inodes_per_block) * sb->inode_size);
}

/* Flush all inode bitmap pages back to disk. */
static void sbfs_flush_inode_bitmap(struct sbfs_superblock_t *sb) {
  uint32_t bpp   = sbfs_blocks_per_page(sb);
  uint32_t pages = (sb->inode_bitmap_num_blocks + bpp - 1) / bpp;
  for (uint32_t p = 0; p < pages; p++) {
    sbfs_write_disk_page(sbfs_block_to_sector(sb, sb->inode_bitmap_start + p * bpp),
                         sb->inode_bitmap_pages[p]);
  }
}

/* Flush all block bitmap pages back to disk. */
static void sbfs_flush_block_bitmap(struct sbfs_superblock_t *sb) {
  uint32_t bpp   = sbfs_blocks_per_page(sb);
  uint32_t pages = (sb->block_bitmap_num_blocks + bpp - 1) / bpp;
  for (uint32_t p = 0; p < pages; p++) {
    sbfs_write_disk_page(sbfs_block_to_sector(sb, sb->block_bitmap_start + p * bpp),
                         sb->block_bitmap_pages[p]);
  }
}

/* Flush only the inode table page that contains `inode_num`. */
static void sbfs_flush_inode(struct sbfs_superblock_t *sb, uint32_t inode_num) {
  uint32_t inodes_per_block = sb->block_size / sb->inode_size;
  uint32_t block_off        = inode_num / inodes_per_block;
  uint32_t bpp              = sbfs_blocks_per_page(sb);
  uint32_t page_idx         = block_off / bpp;
  sbfs_write_disk_page(sbfs_block_to_sector(sb, sb->inode_table_start + page_idx * bpp),
                       sb->inode_table_pages[page_idx]);
}

/* -------------------------------------------------------------------------
 * Data block I/O
 * ---------------------------------------------------------------------- */

/* Read one filesystem block (block_size bytes) into buf. */
static void sbfs_read_data_block(struct sbfs_superblock_t *sb, uint32_t block_num, void *buf) {
  uint32_t bpp            = sbfs_blocks_per_page(sb);
  uint32_t page_start     = block_num - (block_num % bpp);
  uint32_t offset_in_page = (block_num % bpp) * sb->block_size;

  void *phys = get_page(true);
  sbfs_read_page(sbfs_block_to_sector(sb, page_start), phys);
  memcpy(buf, (uint8_t *)PHYS_TO_VIRT(phys) + offset_in_page, sb->block_size);
  free_page(phys);
}

/* Write one filesystem block (block_size bytes) from buf (read-modify-write). */
static void sbfs_write_data_block(struct sbfs_superblock_t *sb, uint32_t block_num, const void *buf) {
  uint32_t bpp            = sbfs_blocks_per_page(sb);
  uint32_t page_start     = block_num - (block_num % bpp);
  uint32_t offset_in_page = (block_num % bpp) * sb->block_size;

  void *phys = get_page(true);
  sbfs_read_page(sbfs_block_to_sector(sb, page_start), phys);
  memcpy((uint8_t *)PHYS_TO_VIRT(phys) + offset_in_page, buf, sb->block_size);
  sbfs_write_disk_page(sbfs_block_to_sector(sb, page_start), phys);
  free_page(phys);
}

/* -------------------------------------------------------------------------
 * Inode and block allocation
 * ---------------------------------------------------------------------- */

/* Find and allocate a free inode. Returns inode number, or -1 if full. */
static int64_t sbfs_alloc_inode(struct sbfs_superblock_t *sb) {
  for (uint32_t i = 0; i < sb->inode_count; i++) {
    uint8_t *byte = sbfs_inode_bitmap_byte(sb, i);
    uint8_t  bit  = 1 << (i % 8);
    if (!(*byte & bit)) {
      *byte |= bit;
      sbfs_flush_inode_bitmap(sb);
      return (int64_t)i;
    }
  }
  return -1;
}

/* Free an inode: clear its bitmap bit and zero the inode struct on disk. */
static void sbfs_free_inode(struct sbfs_superblock_t *sb, uint32_t inode_num) {
  uint8_t *byte = sbfs_inode_bitmap_byte(sb, inode_num);
  *byte &= ~(uint8_t)(1 << (inode_num % 8));
  sbfs_flush_inode_bitmap(sb);

  sbfs_inode_t *inode = sbfs_get_inode(sb, inode_num);
  memset(inode, 0, sb->inode_size);
  sbfs_flush_inode(sb, inode_num);
}

/* Find and allocate a free data block (at or after data_start). Returns block number, or -1 if full. */
static int64_t sbfs_alloc_block(struct sbfs_superblock_t *sb) {
  for (uint32_t b = sb->data_start; b < sb->total_blocks; b++) {
    uint8_t *byte = sbfs_block_bitmap_byte(sb, b);
    uint8_t  bit  = 1 << (b % 8);
    if (!(*byte & bit)) {
      *byte |= bit;
      sbfs_flush_block_bitmap(sb);
      return (int64_t)b;
    }
  }
  return -1;
}

/* Free a data block: clear its bitmap bit. */
static void sbfs_free_block(struct sbfs_superblock_t *sb, uint32_t block_num) {
  uint8_t *byte = sbfs_block_bitmap_byte(sb, block_num);
  *byte &= ~(uint8_t)(1 << (block_num % 8));
  sbfs_flush_block_bitmap(sb);
}

/* -------------------------------------------------------------------------
 * Vnode allocation
 * ---------------------------------------------------------------------- */

static struct vnode_t *sbfs_alloc_vnode(struct superblock_t *superblock, uint32_t inode_num) {
  debugk("[sbfs_alloc_vnode] inode_num=%u superblock=%p\n", inode_num, superblock);
  struct sbfs_superblock_t *sb = (struct sbfs_superblock_t *)superblock->private_data;
  debugk("[sbfs_alloc_vnode] sb=%p\n", sb);

  debugk("[sbfs_alloc_vnode] calling vnode_t_alloc\n");
  struct vnode_t *vnode = vnode_t_alloc();
  debugk("[sbfs_alloc_vnode] vnode=%p calling vfs_init_vnode\n", vnode);
  vfs_init_vnode(vnode, superblock, sb->last_vnode_id++);

  debugk("[sbfs_alloc_vnode] calling sbfs_vnode_t_alloc\n");
  struct sbfs_vnode_t *sv = sbfs_vnode_t_alloc();
  debugk("[sbfs_alloc_vnode] sv=%p\n", sv);
  sv->inode_num = inode_num;
  vnode->fs_private_vnode = sv;

  debugk("[sbfs_alloc_vnode] calling address_space_t_alloc\n");
  struct address_space_t *as = address_space_t_alloc();
  debugk("[sbfs_alloc_vnode] as=%p\n", as);
  as->num_pages_used = 0;
  as->page_cache_list.next = &as->page_cache_list;
  as->page_cache_list.prev = &as->page_cache_list;
  as->address_space_ops = &superblock->address_space_ops;
  vnode->address_space = as;

  debugk("[sbfs_alloc_vnode] calling sbfs_get_inode\n");
  sbfs_inode_t *inode = sbfs_get_inode(sb, inode_num);
  debugk("[sbfs_alloc_vnode] inode=%p size=%u type=%u\n", inode, inode->size, inode->type);
  vnode->size = inode->size;
  vnode->permission_mode = (inode->type == SBFS_INODE_DIR)
    ? (S_IFDIR | READ_EXECUTE_PERM | PERM_WUSR)
    : (S_IFREG | RW_PERM);

  debugk("[sbfs_alloc_vnode] done vnode=%p\n", vnode);
  return vnode;
}

/* -------------------------------------------------------------------------
 * Mount
 * ---------------------------------------------------------------------- */

struct superblock_t *sbfs_mount(void) {
  /* Read the first page (contains the 512-byte disk superblock at offset 0). */
  void *phys = get_page(true);
  sbfs_read_page(0, phys);
  sbfs_disk_superblock_t *disk_sb = (sbfs_disk_superblock_t *)PHYS_TO_VIRT(phys);

  if (disk_sb->magic != SBFS_MAGIC) {
    debugk("sbfs: bad magic 0x%x\n", disk_sb->magic);
    free_page(phys);
    return NULL;
  }

  if (disk_sb->block_size == 0 || disk_sb->block_size > DEFAULT_PAGE_SIZE) {
    debugk("sbfs: unsupported block_size %u\n", disk_sb->block_size);
    free_page(phys);
    return NULL;
  }

  /* Populate in-memory superblock layout. */
  struct sbfs_superblock_t *sb = sbfs_superblock_t_alloc();
  sb->block_size                = disk_sb->block_size;
  sb->total_blocks              = disk_sb->total_blocks;
  sb->inode_count               = disk_sb->inode_count;
  sb->inode_size                = disk_sb->inode_size;
  sb->inode_bitmap_start        = disk_sb->inode_bitmap_start;
  sb->inode_bitmap_num_blocks   = disk_sb->inode_bitmap_num_blocks;
  sb->block_bitmap_start        = disk_sb->block_bitmap_start;
  sb->block_bitmap_num_blocks   = disk_sb->block_bitmap_num_blocks;
  sb->inode_table_start         = disk_sb->inode_table_start;
  sb->inode_table_num_blocks    = disk_sb->inode_table_num_blocks;
  sb->data_start                = disk_sb->data_start;
  sb->last_vnode_id             = 0;
  uint32_t root_inode           = disk_sb->root_inode;

  free_page(phys);

  /* Load all metadata regions into memory. */
  uint32_t blocks_per_page = DEFAULT_PAGE_SIZE / sb->block_size;

  uint32_t inode_bitmap_pages = (sb->inode_bitmap_num_blocks + blocks_per_page - 1) / blocks_per_page;
  for (uint32_t p = 0; p < inode_bitmap_pages; p++) {
    void *meta = get_page(true);
    sbfs_read_page(sbfs_block_to_sector(sb, sb->inode_bitmap_start + p * blocks_per_page), meta);
    sb->inode_bitmap_pages[p] = meta;
  }

  uint32_t block_bitmap_pages = (sb->block_bitmap_num_blocks + blocks_per_page - 1) / blocks_per_page;
  for (uint32_t p = 0; p < block_bitmap_pages; p++) {
    void *meta = get_page(true);
    sbfs_read_page(sbfs_block_to_sector(sb, sb->block_bitmap_start + p * blocks_per_page), meta);
    sb->block_bitmap_pages[p] = meta;
  }

  uint32_t inode_table_pages = (sb->inode_table_num_blocks + blocks_per_page - 1) / blocks_per_page;
  for (uint32_t p = 0; p < inode_table_pages; p++) {
    void *meta = get_page(true);
    sbfs_read_page(sbfs_block_to_sector(sb, sb->inode_table_start + p * blocks_per_page), meta);
    sb->inode_table_pages[p] = meta;
  }

  /* Build the VFS superblock and wire up all ops. */
  struct superblock_t *superblock = superblock_t_alloc();
  superblock->private_data                     = sb;
  superblock->block_size                       = sb->block_size;
  superblock->vnode_ops.lookup                 = sbfs_lookup;
  superblock->vnode_ops.readdir                = sbfs_readdir;
  superblock->vnode_ops.create                 = sbfs_create;
  superblock->vnode_ops.mkdir                  = sbfs_mkdir;
  superblock->vnode_ops.unlink                 = sbfs_unlink;
  superblock->vnode_ops.rmdir                  = sbfs_rmdir;
  superblock->vnode_ops.truncate               = sbfs_truncate;
  superblock->address_space_ops.fill_page      = sbfs_fill_page;
  superblock->address_space_ops.write_page     = sbfs_write_page;
  superblock->file_ops.read                    = NULL;
  superblock->file_ops.write                   = NULL;

  /* Allocate root vnode and dentry. */
  superblock->root_vnode = sbfs_alloc_vnode(superblock, root_inode);

  struct dentry_t *root_dentry = dentry_t_alloc();
  strncpy(root_dentry->name, "/", 256);
  root_dentry->vnode  = superblock->root_vnode;
  root_dentry->parent = NULL;
  superblock->root_dentry = root_dentry;

  return superblock;
}

/* -------------------------------------------------------------------------
 * Directory helpers
 * ---------------------------------------------------------------------- */

/*
 * Scan all direct blocks of parent_inode for a dirent matching `name`.
 * Returns its inode_num (> 0) on success, 0 if not found.
 */
static uint32_t sbfs_dir_find(struct sbfs_superblock_t *sb, sbfs_inode_t *parent_inode,
                               const char *name) {
  uint8_t buf[SECTOR_BLOCK_SIZE];
  uint32_t dents = sb->block_size / sizeof(sbfs_dirent_t);

  for (int i = 0; i < SBFS_DIRECT_BLOCKS; i++) {
    uint32_t blk = parent_inode->direct_blocks[i];
    if (blk == 0) continue;
    sbfs_read_data_block(sb, blk, buf);
    sbfs_dirent_t *de = (sbfs_dirent_t *)buf;
    for (uint32_t j = 0; j < dents; j++) {
      if (de[j].inode_num != 0 && strncmp(de[j].name, name) == 0)
        return de[j].inode_num;
    }
  }
  return 0;
}

/*
 * Add a (inode_num, name) dirent to parent_inode's directory.
 * Finds a free slot in an existing block or allocates a new one.
 * Returns 0 on success, -ENOSPC if full.
 */
static int sbfs_dir_add(struct sbfs_superblock_t *sb, sbfs_inode_t *parent_inode,
                         uint32_t parent_inode_num, uint32_t inode_num, const char *name) {
  uint8_t buf[SECTOR_BLOCK_SIZE];
  uint32_t dents = sb->block_size / sizeof(sbfs_dirent_t);

  for (int i = 0; i < SBFS_DIRECT_BLOCKS; i++) {
    uint32_t blk = parent_inode->direct_blocks[i];
    if (blk == 0) continue;
    sbfs_read_data_block(sb, blk, buf);
    sbfs_dirent_t *de = (sbfs_dirent_t *)buf;
    for (uint32_t j = 0; j < dents; j++) {
      if (de[j].inode_num == 0) {
        de[j].inode_num = inode_num;
        strncpy(de[j].name, name, SBFS_DIRENT_NAME_LEN - 1);
        de[j].name[SBFS_DIRENT_NAME_LEN - 1] = '\0';
        sbfs_write_data_block(sb, blk, buf);
        return 0;
      }
    }
  }

  /* No free slot — need a new data block. */
  int64_t new_blk = sbfs_alloc_block(sb);
  if (new_blk < 0) return -ENOSPC;

  for (int i = 0; i < SBFS_DIRECT_BLOCKS; i++) {
    if (parent_inode->direct_blocks[i] == 0) {
      parent_inode->direct_blocks[i] = (uint32_t)new_blk;
      memset(buf, 0, sb->block_size);
      sbfs_dirent_t *de = (sbfs_dirent_t *)buf;
      de[0].inode_num = inode_num;
      strncpy(de[0].name, name, SBFS_DIRENT_NAME_LEN - 1);
      de[0].name[SBFS_DIRENT_NAME_LEN - 1] = '\0';
      sbfs_write_data_block(sb, (uint32_t)new_blk, buf);
      sbfs_flush_inode(sb, parent_inode_num);
      return 0;
    }
  }

  sbfs_free_block(sb, (uint32_t)new_blk);
  return -ENOSPC;
}

/*
 * Zero out the dirent matching `name` in parent_inode's directory.
 * Returns 0 on success, -ENOENT if not found.
 */
static int sbfs_dir_remove(struct sbfs_superblock_t *sb, sbfs_inode_t *parent_inode,
                            const char *name) {
  uint8_t buf[SECTOR_BLOCK_SIZE];
  uint32_t dents = sb->block_size / sizeof(sbfs_dirent_t);

  for (int i = 0; i < SBFS_DIRECT_BLOCKS; i++) {
    uint32_t blk = parent_inode->direct_blocks[i];
    if (blk == 0) continue;
    sbfs_read_data_block(sb, blk, buf);
    sbfs_dirent_t *de = (sbfs_dirent_t *)buf;
    for (uint32_t j = 0; j < dents; j++) {
      if (de[j].inode_num != 0 && strncmp(de[j].name, name) == 0) {
        memset(&de[j], 0, sizeof(sbfs_dirent_t));
        sbfs_write_data_block(sb, blk, buf);
        return 0;
      }
    }
  }
  return -ENOENT;
}

/* -------------------------------------------------------------------------
 * Vnode ops
 * ---------------------------------------------------------------------- */

int64_t sbfs_lookup(const char *name, struct vnode_t *parent_dir, struct dentry_t **out) {
  struct sbfs_superblock_t *sb = (struct sbfs_superblock_t *)parent_dir->superblock->private_data;
  struct sbfs_vnode_t      *sv = (struct sbfs_vnode_t *)parent_dir->fs_private_vnode;
  sbfs_inode_t *parent_inode = sbfs_get_inode(sb, sv->inode_num);

  uint32_t ino = sbfs_dir_find(sb, parent_inode, name);
  if (ino == 0) {
    *out = NULL;
    return -ENOENT;
  }

  struct dentry_t *dentry = dentry_t_alloc();
  strncpy(dentry->name, name, sizeof(dentry->name) - 1);
  dentry->name[sizeof(dentry->name) - 1] = '\0';
  dentry->vnode  = sbfs_alloc_vnode(parent_dir->superblock, ino);
  dentry->parent = NULL;
  *out = dentry;
  return 0;
}

int64_t sbfs_readdir(struct vnode_t *dir, uint32_t index, struct dentry_t **out) {
  struct sbfs_superblock_t *sb = (struct sbfs_superblock_t *)dir->superblock->private_data;
  struct sbfs_vnode_t      *sv = (struct sbfs_vnode_t *)dir->fs_private_vnode;
  sbfs_inode_t *inode = sbfs_get_inode(sb, sv->inode_num);

  uint8_t  buf[SECTOR_BLOCK_SIZE];
  uint32_t dents = sb->block_size / sizeof(sbfs_dirent_t);
  uint32_t count = 0;

  for (int i = 0; i < SBFS_DIRECT_BLOCKS; i++) {
    uint32_t blk = inode->direct_blocks[i];
    if (blk == 0) continue;
    sbfs_read_data_block(sb, blk, buf);
    sbfs_dirent_t *de = (sbfs_dirent_t *)buf;
    for (uint32_t j = 0; j < dents; j++) {
      if (de[j].inode_num == 0) continue;
      if (count == index) {
        struct dentry_t *dentry = dentry_t_alloc();
        strncpy(dentry->name, de[j].name, sizeof(dentry->name) - 1);
        dentry->name[sizeof(dentry->name) - 1] = '\0';
        dentry->vnode       = NULL;
        dentry->readdir_ino = de[j].inode_num;
        *out = dentry;
        return 0;
      }
      count++;
    }
  }
  *out = NULL;
  return -ENOENT;
}

int64_t sbfs_create(const char *name, struct vnode_t *parent_dir, struct dentry_t **out) {
  struct sbfs_superblock_t *sb = (struct sbfs_superblock_t *)parent_dir->superblock->private_data;
  struct sbfs_vnode_t      *sv = (struct sbfs_vnode_t *)parent_dir->fs_private_vnode;
  sbfs_inode_t *parent_inode = sbfs_get_inode(sb, sv->inode_num);

  if (sbfs_dir_find(sb, parent_inode, name) != 0)
    return -EEXIST;

  int64_t ino = sbfs_alloc_inode(sb);
  if (ino < 0) return -ENOSPC;

  sbfs_inode_t *inode = sbfs_get_inode(sb, (uint32_t)ino);
  memset(inode, 0, sb->inode_size);
  inode->type   = SBFS_INODE_FILE;
  inode->nlinks = 1;
  sbfs_flush_inode(sb, (uint32_t)ino);

  if (sbfs_dir_add(sb, parent_inode, sv->inode_num, (uint32_t)ino, name) < 0) {
    sbfs_free_inode(sb, (uint32_t)ino);
    return -ENOSPC;
  }

  struct dentry_t *dentry = dentry_t_alloc();
  strncpy(dentry->name, name, sizeof(dentry->name) - 1);
  dentry->name[sizeof(dentry->name) - 1] = '\0';
  dentry->vnode  = sbfs_alloc_vnode(parent_dir->superblock, (uint32_t)ino);
  dentry->parent = NULL;
  *out = dentry;
  return 0;
}

int64_t sbfs_mkdir(const char *name, struct vnode_t *parent_dir, struct dentry_t **out) {
  debugk("[sbfs_mkdir] name='%s' parent_dir=%p\n", name, parent_dir);
  struct sbfs_superblock_t *sb = (struct sbfs_superblock_t *)parent_dir->superblock->private_data;
  struct sbfs_vnode_t      *sv = (struct sbfs_vnode_t *)parent_dir->fs_private_vnode;
  debugk("[sbfs_mkdir] sb=%p sv=%p inode_num=%u\n", sb, sv, sv->inode_num);
  sbfs_inode_t *parent_inode = sbfs_get_inode(sb, sv->inode_num);
  debugk("[sbfs_mkdir] parent_inode=%p\n", parent_inode);

  if (sbfs_dir_find(sb, parent_inode, name) != 0)
    return -EEXIST;
  debugk("[sbfs_mkdir] dir_find: not found, proceeding\n");

  int64_t ino = sbfs_alloc_inode(sb);
  debugk("[sbfs_mkdir] alloc_inode returned %lld\n", ino);
  if (ino < 0) return -ENOSPC;

  sbfs_inode_t *inode = sbfs_get_inode(sb, (uint32_t)ino);
  debugk("[sbfs_mkdir] new inode ptr=%p\n", inode);
  memset(inode, 0, sb->inode_size);
  inode->type   = SBFS_INODE_DIR;
  inode->nlinks = 1;
  debugk("[sbfs_mkdir] flushing inode %lld\n", ino);
  sbfs_flush_inode(sb, (uint32_t)ino);

  debugk("[sbfs_mkdir] calling dir_add parent_inode_num=%u new_ino=%lld\n", sv->inode_num, ino);
  if (sbfs_dir_add(sb, parent_inode, sv->inode_num, (uint32_t)ino, name) < 0) {
    sbfs_free_inode(sb, (uint32_t)ino);
    return -ENOSPC;
  }
  debugk("[sbfs_mkdir] dir_add done\n");

  debugk("[sbfs_mkdir] calling dentry_t_alloc\n");
  struct dentry_t *dentry = dentry_t_alloc();
  debugk("[sbfs_mkdir] dentry=%p\n", dentry);
  strncpy(dentry->name, name, sizeof(dentry->name) - 1);
  dentry->name[sizeof(dentry->name) - 1] = '\0';
  debugk("[sbfs_mkdir] calling sbfs_alloc_vnode ino=%lld\n", ino);
  dentry->vnode  = sbfs_alloc_vnode(parent_dir->superblock, (uint32_t)ino);
  debugk("[sbfs_mkdir] alloc_vnode done vnode=%p\n", dentry->vnode);
  dentry->parent = NULL;
  *out = dentry;
  debugk("[sbfs_mkdir] done\n");
  return 0;
}

int64_t sbfs_unlink(const char *name, struct vnode_t *parent_dir) {
  struct sbfs_superblock_t *sb = (struct sbfs_superblock_t *)parent_dir->superblock->private_data;
  struct sbfs_vnode_t      *sv = (struct sbfs_vnode_t *)parent_dir->fs_private_vnode;
  sbfs_inode_t *parent_inode = sbfs_get_inode(sb, sv->inode_num);

  uint32_t ino = sbfs_dir_find(sb, parent_inode, name);
  if (ino == 0) return -ENOENT;

  sbfs_inode_t *inode = sbfs_get_inode(sb, ino);
  if (inode->type == SBFS_INODE_DIR) return -EISDIR;
  if (inode->nlinks == 0) return -EINVAL;

  inode->nlinks--;
  if (inode->nlinks == 0) {
    for (int i = 0; i < SBFS_DIRECT_BLOCKS; i++) {
      if (inode->direct_blocks[i] != 0)
        sbfs_free_block(sb, inode->direct_blocks[i]);
    }
    sbfs_free_inode(sb, ino);
  } else {
    sbfs_flush_inode(sb, ino);
  }

  return sbfs_dir_remove(sb, parent_inode, name);
}

int64_t sbfs_rmdir(const char *name, struct vnode_t *parent_dir) {
  struct sbfs_superblock_t *sb = (struct sbfs_superblock_t *)parent_dir->superblock->private_data;
  struct sbfs_vnode_t      *sv = (struct sbfs_vnode_t *)parent_dir->fs_private_vnode;
  sbfs_inode_t *parent_inode = sbfs_get_inode(sb, sv->inode_num);

  uint32_t ino = sbfs_dir_find(sb, parent_inode, name);
  if (ino == 0) return -ENOENT;

  sbfs_inode_t *inode = sbfs_get_inode(sb, ino);
  if (inode->type != SBFS_INODE_DIR) return -ENOTDIR;

  /* Refuse to remove a non-empty directory. */
  uint8_t  buf[SECTOR_BLOCK_SIZE];
  uint32_t dents = sb->block_size / sizeof(sbfs_dirent_t);
  for (int i = 0; i < SBFS_DIRECT_BLOCKS; i++) {
    if (inode->direct_blocks[i] == 0) continue;
    sbfs_read_data_block(sb, inode->direct_blocks[i], buf);
    sbfs_dirent_t *de = (sbfs_dirent_t *)buf;
    for (uint32_t j = 0; j < dents; j++) {
      if (de[j].inode_num != 0) return -ENOTEMPTY;
    }
  }

  for (int i = 0; i < SBFS_DIRECT_BLOCKS; i++) {
    if (inode->direct_blocks[i] != 0)
      sbfs_free_block(sb, inode->direct_blocks[i]);
  }
  sbfs_free_inode(sb, ino);

  return sbfs_dir_remove(sb, parent_inode, name);
}

/* -------------------------------------------------------------------------
 * Address space ops
 * ---------------------------------------------------------------------- */

int64_t sbfs_fill_page(struct vnode_t *vnode, size_t offset, void **phys_page) {
  struct sbfs_superblock_t *sb = (struct sbfs_superblock_t *)vnode->superblock->private_data;
  struct sbfs_vnode_t      *sv = (struct sbfs_vnode_t *)vnode->fs_private_vnode;
  sbfs_inode_t *inode = sbfs_get_inode(sb, sv->inode_num);

  void    *phys = get_page(true);
  uint8_t *virt = (uint8_t *)PHYS_TO_VIRT(phys);
  uint32_t bpp  = sbfs_blocks_per_page(sb);
  uint32_t file_block_start = offset / sb->block_size;

  for (uint32_t i = 0; i < bpp; i++) {
    uint32_t fb = file_block_start + i;
    if (fb >= SBFS_DIRECT_BLOCKS) break;
    uint32_t disk_blk = inode->direct_blocks[fb];
    if (disk_blk == 0) continue;
    sbfs_read_data_block(sb, disk_blk, virt + i * sb->block_size);
  }

  *phys_page = phys;
  return 0;
}

int64_t sbfs_write_page(struct vnode_t *vnode, size_t offset, void *phys_page) {
  struct sbfs_superblock_t *sb = (struct sbfs_superblock_t *)vnode->superblock->private_data;
  struct sbfs_vnode_t      *sv = (struct sbfs_vnode_t *)vnode->fs_private_vnode;
  sbfs_inode_t *inode = sbfs_get_inode(sb, sv->inode_num);

  uint8_t *virt = (uint8_t *)PHYS_TO_VIRT(phys_page);
  uint32_t bpp  = sbfs_blocks_per_page(sb);
  uint32_t file_block_start = offset / sb->block_size;
  bool     inode_dirty = false;

  for (uint32_t i = 0; i < bpp; i++) {
    uint32_t fb = file_block_start + i;
    if (fb >= SBFS_DIRECT_BLOCKS) {
      if (inode_dirty)
        sbfs_flush_inode(sb, sv->inode_num);
      return -EFBIG;
    }

    if (inode->direct_blocks[fb] == 0) {
      int64_t new_blk = sbfs_alloc_block(sb);
      if (new_blk < 0) {
        if (inode_dirty)
          sbfs_flush_inode(sb, sv->inode_num);
        return -ENOSPC;
      }
      inode->direct_blocks[fb] = (uint32_t)new_blk;
      inode_dirty = true;
    }

    sbfs_write_data_block(sb, inode->direct_blocks[fb], virt + i * sb->block_size);
  }

  /* Persist the new file size. vnode->size is updated by vfs_vnode_write *after*
   * write_page returns, so we derive the minimum new size from the page we just
   * wrote and take the max against the current on-disk size. */
  size_t new_size = offset + (size_t)bpp * sb->block_size;
  if (new_size > (size_t)SBFS_DIRECT_BLOCKS * sb->block_size)
    new_size = (size_t)SBFS_DIRECT_BLOCKS * sb->block_size;
  if (new_size > inode->size) {
    inode->size = new_size;
    inode_dirty = true;
  }

  if (inode_dirty)
    sbfs_flush_inode(sb, sv->inode_num);

  return 0;
}

static int64_t sbfs_truncate(struct vnode_t *vnode, uint64_t new_size) {
  struct sbfs_superblock_t *sb = (struct sbfs_superblock_t *)vnode->superblock->private_data;
  struct sbfs_vnode_t      *sv = (struct sbfs_vnode_t *)vnode->fs_private_vnode;
  sbfs_inode_t *inode = sbfs_get_inode(sb, sv->inode_num);

  uint32_t first_block_to_free = (uint32_t)((new_size + sb->block_size - 1) / sb->block_size);

  for (uint32_t i = first_block_to_free; i < SBFS_DIRECT_BLOCKS; i++) {
    if (inode->direct_blocks[i] != 0) {
      sbfs_free_block(sb, inode->direct_blocks[i]);
      inode->direct_blocks[i] = 0;
    }
  }

  inode->size = (uint32_t)new_size;
  sbfs_flush_inode(sb, sv->inode_num);

  vnode->size = new_size;
  return 0;
}
