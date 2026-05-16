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

/*
 * Add an in-memory directory stub to a parent vnode's children_dentries list
 * and point it at a target vnode via mounted_vnode.  This is how mount points
 * for tarfs subtrees, devfs, and procfs are grafted onto the sbfs root without
 * those filesystems needing to know about each other.
 */
static void add_mount_stub(struct superblock_t *alloc_sb,
                            struct vnode_t *parent_vnode,
                            struct dentry_t *parent_dentry,
                            const char *name,
                            struct vnode_t *target_vnode) {
  struct vnode_t *stub = tarfs_alloc_vnode(alloc_sb);
  stub->permission_mode = READ_EXECUTE_PERM | S_IFDIR;
  stub->mounted_vnode   = target_vnode;

  struct dentry_t *de = dentry_t_alloc();
  strncpy(de->name, name, 256);
  de->vnode  = stub;
  de->parent = parent_dentry;
  list_append(&parent_vnode->children_dentries, &de->sibling_dentry);
}

void vfs_init() {
  mount_list.next = &mount_list;
  mount_list.prev = &mount_list;

  /* Always build the tarfs tree — it holds /bin and /etc content. */
  struct superblock_t *tarfs_sb = tarfs_mount((void *)_tarfs_start, (uint64_t)_tarfs_size);

  base_mount = mount_t_alloc();
  base_mount->root_path[0] = '/';
  base_mount->root_path[1] = '\0';
  list_append(&mount_list, &base_mount->sibling_mount);

  /* Try to bring up the disk. */
  struct superblock_t *sbfs_sb = sbfs_mount();

  if (sbfs_sb == NULL) {
    /*
     * Fallback: no disk available — tarfs stays as root, same as before.
     * Wire dev and proc stubs directly into the tarfs tree.
     */
    printk("vfs: sbfs unavailable, using tarfs as root\n");
    base_mount->superblock = tarfs_sb;

    add_mount_stub(tarfs_sb, tarfs_sb->root_vnode, tarfs_sb->root_dentry,
                   "dev", devfs_mount()->root_vnode);
    add_mount_stub(tarfs_sb, tarfs_sb->root_vnode, tarfs_sb->root_dentry,
                   "proc", procfs_mount()->root_vnode);
    return;
  }

  /*
   * Normal path: sbfs is the root filesystem.
   * Make the sbfs root dentry self-referential so that vfs_dentry_get_path
   * recognises it as the root (same convention as tarfs uses).
   */
  sbfs_sb->root_dentry->parent = sbfs_sb->root_dentry;
  base_mount->superblock = sbfs_sb;

  /* Find the tarfs bin/ and etc/ vnodes to graft them onto the sbfs root. */
  struct vnode_t *tarfs_bin = NULL;
  struct vnode_t *tarfs_etc = NULL;
  list_for_each(&tarfs_sb->root_vnode->children_dentries, pos) {
    struct dentry_t *d = container_of(pos, struct dentry_t, sibling_dentry);
    if (strncmp(d->name, "bin") == 0) tarfs_bin = d->vnode;
    if (strncmp(d->name, "etc") == 0) tarfs_etc = d->vnode;
  }

  struct vnode_t *root  = sbfs_sb->root_vnode;
  struct dentry_t *root_de = sbfs_sb->root_dentry;

  if (tarfs_bin) add_mount_stub(tarfs_sb, root, root_de, "bin", tarfs_bin);
  if (tarfs_etc) add_mount_stub(tarfs_sb, root, root_de, "etc", tarfs_etc);

  add_mount_stub(tarfs_sb, root, root_de, "dev",  devfs_mount()->root_vnode);
  add_mount_stub(tarfs_sb, root, root_de, "proc", procfs_mount()->root_vnode);

  printk("vfs: sbfs at /, bin/etc from tarfs, dev/proc virtual\n");
}
