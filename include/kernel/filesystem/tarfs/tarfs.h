#ifndef TARFS_H
#define TARFS_H

#include "kernel/filesystem/vfs.h"
#include "types.h"
#include "lib/pool_allocator.h"

extern char _tarfs_start[];
extern char _tarfs_end[];
extern char _tarfs_size[];

struct tarfs_vnode_t {
  void *data;
};

DEFINE_POOL(tarfs_vnode_t, struct tarfs_vnode_t)

struct superblock_t *tarfs_mount(void *data, uint64_t size);

#endif
