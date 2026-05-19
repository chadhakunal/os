#include "kernel/filesystem/devfs/devfs.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/drivers/tty.h"
#include "kernel/drivers/virtio-blk.h"
#include "kernel/filesystem/mode.h"
#include "lib/list.h"
#include "lib/string.h"
#include "errno.h"

static int64_t null_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  (void)file; (void)offset; (void)buffer; (void)size;
  return 0; /* always EOF */
}

static int64_t null_write(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  (void)file; (void)offset; (void)buffer;
  return (int64_t)size; /* discard, claim success */
}

static struct file_ops_t null_file_ops = {
  .read  = null_read,
  .write = null_write,
  .ioctl = NULL,
};

static int64_t vda_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  uint64_t sector = offset / SECTOR_BLOCK_SIZE;
  uint32_t num_sectors = (size + SECTOR_BLOCK_SIZE - 1) / SECTOR_BLOCK_SIZE;
  return virtio_blk_read(sector, buffer, num_sectors) == 0 ? (int64_t)size : -1;
}

static int64_t vda_write(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  uint64_t sector = offset / SECTOR_BLOCK_SIZE;
  uint32_t num_sectors = (size + SECTOR_BLOCK_SIZE - 1) / SECTOR_BLOCK_SIZE;
  return virtio_blk_write(sector, buffer, num_sectors) == 0 ? (int64_t)size : -1;
}

static struct file_ops_t vda_file_ops = {
  .read  = vda_read,
  .write = vda_write,
  .ioctl = NULL,
};

static int64_t devfs_lookup(const char *name, struct vnode_t *dir, struct dentry_t **out) {
  list_for_each(&dir->children_dentries, pos) {
    struct dentry_t *d = container_of(pos, struct dentry_t, sibling_dentry);
    if (strncmp(d->name, name) == 0) {
      *out = d;
      return 0;
    }
  }
  *out = NULL;
  return -ENOENT;
}

static int64_t devfs_create(const char *name, struct vnode_t *dir,
                             struct dentry_t **out, uint32_t mode) {
  (void)mode;
  /* devfs entries are fixed — existing name → EEXIST, new name → EPERM */
  if (devfs_lookup(name, dir, out) == 0)
    return -EEXIST;
  *out = NULL;
  return -EPERM;
}

static int64_t devfs_readdir(struct vnode_t *dir, uint32_t index, struct dentry_t **out) {
  uint32_t i = 0;
  list_for_each(&dir->children_dentries, pos) {
    if (i == index) {
      *out = container_of(pos, struct dentry_t, sibling_dentry);
      return 0;
    }
    i++;
  }
  *out = NULL;
  return -1;
}

struct vnode_t *build_devfs(struct superblock_t *superblock) {
  struct vnode_t *root_vnode = vnode_t_alloc();
  uint32_t id = 0;
  vfs_init_vnode(root_vnode, superblock, id++);
  root_vnode->permission_mode = S_IFDIR | 0777;
  
  struct vnode_t *tty_vnode = vnode_t_alloc();
  vfs_init_vnode(tty_vnode, superblock, id++);
  tty_vnode->permission_mode = RW_PERM | S_IFCHR;

  // Set up TTY file operations
  tty_vnode->file_ops = &tty_driver.file_ops;

  struct dentry_t *tty_dentry = dentry_t_alloc();
  strncpy(tty_dentry->name, "tty", 256);
  tty_dentry->vnode = tty_vnode;
  tty_dentry->parent = NULL;  // Will be set at mount time

  list_append(&root_vnode->children_dentries, &tty_dentry->sibling_dentry);

  struct vnode_t *vda_vnode = vnode_t_alloc();
  vfs_init_vnode(vda_vnode, superblock, id++);
  vda_vnode->permission_mode = RW_PERM | S_IFBLK;
  vda_vnode->file_ops = &vda_file_ops;

  struct dentry_t *vda_dentry = dentry_t_alloc();
  strncpy(vda_dentry->name, "vda", 256);
  vda_dentry->vnode = vda_vnode;
  vda_dentry->parent = NULL;

  list_append(&root_vnode->children_dentries, &vda_dentry->sibling_dentry);

  struct vnode_t *null_vnode = vnode_t_alloc();
  vfs_init_vnode(null_vnode, superblock, id++);
  null_vnode->permission_mode = RW_PERM | S_IFCHR;
  null_vnode->file_ops = &null_file_ops;

  struct dentry_t *null_dentry = dentry_t_alloc();
  strncpy(null_dentry->name, "null", 256);
  null_dentry->vnode = null_vnode;
  null_dentry->parent = NULL;

  list_append(&root_vnode->children_dentries, &null_dentry->sibling_dentry);

  return root_vnode;
}

struct superblock_t *devfs_mount() {
  struct superblock_t *superblock = superblock_t_alloc();
  // devfs doesn't use superblock file_ops (devices override per-vnode)
  superblock->file_ops.read = NULL;
  superblock->file_ops.write = NULL;
  superblock->vnode_ops.lookup  = devfs_lookup;
  superblock->vnode_ops.create  = devfs_create;
  superblock->vnode_ops.readdir = devfs_readdir;

  superblock->root_vnode = build_devfs(superblock);

  // Create root dentry for devfs
  struct dentry_t *root_dentry = dentry_t_alloc();
  strncpy(root_dentry->name, "dev", 256);
  root_dentry->vnode = superblock->root_vnode;
  root_dentry->parent = NULL;  // Will be set at mount time
  superblock->root_dentry = root_dentry;

  return superblock;
}
