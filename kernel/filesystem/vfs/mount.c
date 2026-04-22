#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/tarfs/tarfs.h"
#include "kernel/filesystem/devfs/devfs.h"
#include "lib/list.h"
#include "lib/string.h"

struct mount_t *base_mount = NULL;
struct list_node mount_list;

int32_t vfs_mount(char *path, struct superblock_t *superblock) {
  struct dentry_t *dentry;
  int32_t ret = vfs_resolve_path(path, &dentry);
  if (ret < 0) {
    return ret;
  }

  dentry->vnode->mounted_vnode = superblock->root_vnode;

  // Set parent dentry for all children in the mounted filesystem's root
  list_for_each(&superblock->root_vnode->children_dentries, pos) {
    struct dentry_t *child_dentry = container_of(pos, struct dentry_t, sibling_dentry);
    child_dentry->parent = dentry;
  }

  // Add mount to mount list
  struct mount_t *mount = mount_t_alloc();
  strncpy(mount->root_path, path, 256);
  mount->superblock = superblock;
  list_append(&mount_list, &mount->sibling_mount);

  return 0;
}

void vfs_init() {
  /*
  * Here we will be mounting tarfs as the root fs
  *
  *
  */
  // Initialize mount list
  mount_list.next = &mount_list;
  mount_list.prev = &mount_list;

  struct mount_t *tarfs_mount_obj = mount_t_alloc();
  tarfs_mount_obj->root_path[0] = '/';
  tarfs_mount_obj->root_path[1] = '\0';
  tarfs_mount_obj->superblock = tarfs_mount((void *) _tarfs_start, (uint64_t) _tarfs_size);

  // Add to mount list
  list_append(&mount_list, &tarfs_mount_obj->sibling_mount);

  struct mount_t *devfs_mount_obj = mount_t_alloc();
  strncpy(devfs_mount_obj->root_path, "/dev", 256);
  devfs_mount_obj->superblock = devfs_mount();
  list_append(&mount_list, &devfs_mount_obj->sibling_mount);
}
