#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/tarfs/tarfs.h"
#include "kernel/filesystem/devfs/devfs.h"
#include "lib/list.h"
#include "lib/string.h"
#include "lib/printk/printk.h"

struct mount_t *base_mount = NULL;
struct list_node mount_list;

int32_t vfs_mount(char *path, struct superblock_t *superblock) {
  struct dentry_t *dentry;
  printk("mounting: %s\n", path);
  int32_t ret = vfs_resolve_path(path, &dentry);
  printk("mounting resolve path return: %ld\n", ret);
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

  base_mount = mount_t_alloc();
  base_mount->root_path[0] = '/';
  base_mount->root_path[1] = '\0';
  base_mount->superblock = tarfs_mount((void *) _tarfs_start, (uint64_t) _tarfs_size);

  // Add to mount list
  list_append(&mount_list, &base_mount->sibling_mount);

  // Mount devfs at /dev
  struct superblock_t *devfs_sb = devfs_mount();
  vfs_mount("/dev", devfs_sb);
}
