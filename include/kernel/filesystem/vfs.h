#ifndef VFS_H
#define VFS_H

#include "types.h"
#include "kernel/filesystem/mode.h"
#include "lib/pool_allocator.h"

#define MAX_DENTRIES 256

struct mount_t;
struct superblock_t;
struct vnode_t;
struct dentry_t;

extern struct mount_t *base_mount;

struct mount_t {
  char root_path[256];
  struct superblock_t superblock;
  struct mount_t *next_mount;
};

struct superblock_t {
  struct fs_ops_t *ops;
  struct vnode_t *root_vnode;
  struct dentry_t *root_dentry;
  uint64_t block_size;
  char device[32]; // Will probably just be tarfs or virtio block
};

struct vnode_t {
  uint64_t size;
  uint32_t id;
  uint32_t refcount;
  uint32_t owner_uid;
  uint32_t owner_gid;
  mode_t permission_mode;
  struct superblock_t *superblock;
  struct dentry_t *first_child_dentry; // points to the first dentry in a linked list only if the inode is a directory
  struct dentry_t *last_child_dentry;
  void *fs_private_vnode;
};

struct dentry_t {
  char name[256];
  struct vnode_t *vnode;

  struct dentry_t *parent;
  struct dentry_t *sibling_dentry;
};

DEFINE_POOL(mount_t, struct mount_t)
DEFINE_POOL(superblock_t, struct superblock_t)
DEFINE_POOL(vnode_t, struct vnode_t)
DEFINE_POOL(dentry_t, struct dentry_t)

void vfs_init();

void vfs_mount(char *path, struct superblock_t *superblock);

struct dentry_t *vfs_get_or_create_dentry(struct vnode_t *root_vnode);

#endif
