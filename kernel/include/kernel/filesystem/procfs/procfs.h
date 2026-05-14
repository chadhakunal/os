#ifndef PROCFS_H
#define PROCFS_H

#include "kernel/filesystem/vfs/vfs.h"

struct superblock_t *procfs_mount(void);

#endif
