#include "kernel/filesystem/vfs.h"

struct mount_t *base_mount = NULL;

void vfs_mount(char *path, struct superblock_t *superblock) {
  /*
  * to mount an fs, we need to walk the path first
  * then once we get the inode of the folder which we are mounting onto
  * we replace the folder's inode with the inode of the root inode of the superblock
  * this means we now need a vfs_lookup method
  * We also need to consider the base case where we do not have anything mounted, this should probably be handled in vfs_init actually
  *
  *
  */
}

void vfs_init() {
  /*
  * Here we will be mounting tarfs as the root fs
  *
  *
  */
  base_mount = mount_t_alloc();
  base_mount->root_path[0] = '/';
  base_mount->root_path[1] = '\0';
}
