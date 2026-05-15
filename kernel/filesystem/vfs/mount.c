#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/tarfs/tarfs.h"
#include "kernel/filesystem/devfs/devfs.h"
#include "kernel/filesystem/procfs/procfs.h"
#include "kernel/filesystem/sbfs/sbfs.h"
#include "kernel/filesystem/mode.h"
#include "lib/list.h"
#include "lib/string.h"
#include "lib/printk/printk.h"

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
  // Here we will be mounting tarfs as the root fs
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
  struct vnode_t *dev_vnode = tarfs_alloc_vnode(base_mount->superblock);
  struct dentry_t *dentry = dentry_t_alloc();
  strncpy(dentry->name, "dev", 256);
  dentry->vnode = dev_vnode;
  dentry->parent = base_mount->superblock->root_dentry;
  list_append(&base_mount->superblock->root_vnode->children_dentries, &dentry->sibling_dentry);
  struct superblock_t *devfs_sb = devfs_mount();
  vfs_mount("/dev", devfs_sb);

  // Create /mnt mountpoint in the root tarfs tree
  struct vnode_t *mnt_vnode = tarfs_alloc_vnode(base_mount->superblock);
  mnt_vnode->permission_mode = RW_PERM | S_IFDIR;
  struct dentry_t *mnt_dentry = dentry_t_alloc();
  strncpy(mnt_dentry->name, "mnt", 256);
  mnt_dentry->vnode = mnt_vnode;
  mnt_dentry->parent = base_mount->superblock->root_dentry;
  list_append(&base_mount->superblock->root_vnode->children_dentries, &mnt_dentry->sibling_dentry);

  // Create /proc mountpoint and mount procfs
  struct vnode_t *proc_vnode = tarfs_alloc_vnode(base_mount->superblock);
  proc_vnode->permission_mode = READ_EXECUTE_PERM | S_IFDIR;
  struct dentry_t *proc_dentry = dentry_t_alloc();
  strncpy(proc_dentry->name, "proc", 256);
  proc_dentry->vnode = proc_vnode;
  proc_dentry->parent = base_mount->superblock->root_dentry;
  list_append(&base_mount->superblock->root_vnode->children_dentries, &proc_dentry->sibling_dentry);
  struct superblock_t *proc_sb = procfs_mount();
  vfs_mount("/proc", proc_sb);
  printk("Mounted procfs at /proc\n");

  // Mount sbfs at /mnt
  struct superblock_t *sbfs_sb = sbfs_mount();
  if (sbfs_sb != NULL) {
    vfs_mount("/mnt", sbfs_sb);
    printk("Mounted sbfs at /mnt\n");
  } else {
    printk("sbfs mount failed\n");
  }
}
