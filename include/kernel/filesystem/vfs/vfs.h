#ifndef VFS_H
#define VFS_H

#include "types.h"
#include "kernel/filesystem/mode.h"
#include "lib/pool_allocator.h"
#include "lib/list.h"

#define MAX_DENTRIES 256

extern struct mount_t *base_mount;

struct vnode_t;

struct vnode_ops_t {
  int64_t (*read)(struct vnode_t *vnode, void *buffer, uint64_t offset, uint64_t size);
};

struct dentry_t {
  char name[256];
  struct vnode_t *vnode;

  struct dentry_t *parent;
  struct list_node sibling_dentry;
};

struct vnode_t {
  uint64_t size;
  uint32_t id;
  uint32_t refcount;
  uint32_t owner_uid;
  uint32_t owner_gid;
  mode_t permission_mode;
  struct superblock_t *superblock;
  struct list_node children_dentries; // Sentinel node into the dentry list of children, null if not a directory
  struct vnode_ops_t *ops;
  void *fs_private_vnode;
};

struct superblock_t {
  struct vnode_ops_t vnode_ops;
  struct vnode_t *root_vnode;
  struct dentry_t *root_dentry;
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

void init_vnode(struct vnode_t *vnode, struct superblock_t *sb, uint32_t id, mode_t permission_mode, uint64_t size);
void vfs_print_vnode(struct vnode_t *vnode);
void vfs_print_dentry(struct dentry_t *dentry);

void vfs_init();
void vfs_mount(char *path, struct superblock_t *superblock);

int32_t vfs_resolve_path(const char *path, struct dentry_t **out);
int32_t vfs_lookup(const char *name, struct dentry_t *parent_dir, struct dentry_t **out);

#endif
