#ifndef TARFS_H
#define TARFS_H

#include "kernel/filesystem/vfs/vfs.h"
#include "types.h"
#include "lib/pool_allocator.h"

extern char _tarfs_start[];
extern char _tarfs_end[];
extern char _tarfs_size[];

struct tarfs_vnode_t {
  void *data;
};

struct tarfs_superblock_t {
  int64_t last_vnode_id;
};

DEFINE_POOL(tarfs_vnode_t, struct tarfs_vnode_t)
DEFINE_POOL(tarfs_superblock_t, struct tarfs_superblock_t)

struct superblock_t *tarfs_mount(void *data, uint64_t size);

int64_t tarfs_vnode_read(struct vnode_t *vnode, void *buffer, uint64_t offset, uint64_t size);

#endif
