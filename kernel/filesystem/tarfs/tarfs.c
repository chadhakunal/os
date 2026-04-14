#include "kernel/filesystem/tarfs.h"
#include "kernel/filesystem/tarfs_parser.h"
#include "types.h"

struct superblock_t *tarfs_mount(void *data, uint64_t size) {
  struct superblock_t *tarfs_superblock = superblock_t_alloc();
  parse_tar(data, size, tarfs_superblock);
  return tarfs_superblock;
}
