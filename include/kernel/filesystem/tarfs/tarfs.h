#ifndef TARFS_H
#define TARFS_H

#include "kernel/filesystem/vfs.h"
#include "types.h"
#include "lib/pool_allocator.h"

struct tarfs_vnode_t {
  void *data_start;
};

DEFINE_POOL(tarfs_vnode_t, struct tarfs_vnode_t)

struct superblock_t *tarfs_mount(void *data, uint64_t size);

#endif
