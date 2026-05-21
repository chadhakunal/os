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

/* Writable /tmp on the sbfs root (tarfs bin/etc are read-only grafts). */
// static void vfs_ensure_tmp_dir(struct vnode_t *root, struct dentry_t *root_de) {
//   struct dentry_t *tmp_de = NULL;

//   if (vfs_lookup("tmp", root_de, &tmp_de) == 0 && tmp_de != NULL) {
//     struct vnode_t *v = tmp_de->vnode->mounted_vnode ? tmp_de->vnode->mounted_vnode
//                                                      : tmp_de->vnode;
//     v->permission_mode = S_IFDIR | 01777;
//     return;
//   }

//   if (vfs_mkdir("tmp", root, &tmp_de) == 0 && tmp_de != NULL) {
//     struct vnode_t *v = tmp_de->vnode->mounted_vnode ? tmp_de->vnode->mounted_vnode
//                                                      : tmp_de->vnode;
//     v->permission_mode = S_IFDIR | 01777;
//     tmp_de->parent = root_de;
//     list_append(&root->children_dentries, &tmp_de->sibling_dentry);
//   }
// }

/*
 * Graft a filesystem's root vnode onto the VFS tree at `path`.
 * The target directory must already exist in the tree.
 * This is what the mount(2) syscall calls after resolving the superblock.
 */
int32_t vfs_mount_at(const char *path, struct superblock_t *superblock) {
  struct dentry_t *target = NULL;
  int32_t ret = vfs_resolve_path(path, &target);
  if (ret < 0)
    return ret;

  struct vnode_t *mount_pt = target->vnode->mounted_vnode
                             ? target->vnode->mounted_vnode
                             : target->vnode;
  if (!IS_DIR(mount_pt->permission_mode))
    return -ENOTDIR;

  /* Make the new root's dentry self-referential so ".." stops here. */
  superblock->root_dentry->parent = superblock->root_dentry;

  /* Redirect the mount-point vnode to the new filesystem's root. */
  target->vnode->mounted_vnode = superblock->root_vnode;

  struct mount_t *mount = mount_t_alloc();
  strncpy(mount->root_path, path, 256);
  mount->superblock = superblock;
  list_append(&mount_list, &mount->sibling_mount);

  return 0;
}

/* The one sbfs superblock; sys_mount hands it to vfs_mount_at(). */
static struct superblock_t *g_sbfs_sb = NULL;

struct superblock_t *vfs_get_sbfs(void) {
  return g_sbfs_sb;
}

void vfs_init() {
  mount_list.next = &mount_list;
  mount_list.prev = &mount_list;

  /*
   * tarfs is always the root — read-only, holds /bin and /etc.
   */
  struct superblock_t *tarfs_sb = tarfs_mount((void *)_tarfs_start,
                                              (uint64_t)(_tarfs_end - _tarfs_start));

  base_mount = mount_t_alloc();
  base_mount->root_path[0] = '/';
  base_mount->root_path[1] = '\0';
  base_mount->superblock   = tarfs_sb;
  list_append(&mount_list, &base_mount->sibling_mount);

  struct vnode_t  *root    = tarfs_sb->root_vnode;
  struct dentry_t *root_de = tarfs_sb->root_dentry;

  add_mount_stub(tarfs_sb, root, root_de, "dev",  devfs_mount()->root_vnode);
  add_mount_stub(tarfs_sb, root, root_de, "proc", procfs_mount()->root_vnode);

  /* Pre-create the /mnt stub so userspace can see it even before mount(2). */
  struct vnode_t *mnt_stub = tarfs_alloc_vnode(tarfs_sb);
  mnt_stub->permission_mode = S_IFDIR | 0755;

  struct dentry_t *mnt_de = dentry_t_alloc();
  strncpy(mnt_de->name, "mnt", 256);
  mnt_de->vnode  = mnt_stub;
  mnt_de->parent = root_de;
  list_append(&root->children_dentries, &mnt_de->sibling_dentry);

  /* Try to bring up the block device. */
  struct superblock_t *sbfs_sb = sbfs_mount();
  if (sbfs_sb == NULL) {
    printk("vfs: sbfs unavailable, /mnt is empty\n");
    return;
  }

  g_sbfs_sb = sbfs_sb;

  /*
   * Wire sbfs at /mnt now so the kernel itself and any early-boot code
   * can use /mnt without waiting for a userspace mount(2) call.
   */
  sbfs_sb->root_dentry->parent = sbfs_sb->root_dentry;
  mnt_stub->mounted_vnode = sbfs_sb->root_vnode;

  struct mount_t *mnt_mount = mount_t_alloc();
  strncpy(mnt_mount->root_path, "/mnt", 256);
  mnt_mount->superblock = sbfs_sb;
  list_append(&mount_list, &mnt_mount->sibling_mount);

  printk("vfs: tarfs at /, sbfs at /mnt, dev/proc virtual\n");
}

/* Flush all dirty page-cache pages in one vnode. */
static void sync_vnode(struct vnode_t *vnode) {
  if (vnode == NULL || vnode->address_space == NULL)
    return;

  struct address_space_ops_t *as_ops = vnode->address_space->address_space_ops;
  if (as_ops == NULL || as_ops->write_page == NULL)
    return;

  list_for_each(&vnode->address_space->page_cache_list, pos) {
    struct page_cache_entry_t *entry =
      container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
    if (entry->dirty) {
      as_ops->write_page(vnode, entry->offset, entry->physical_page);
      entry->dirty = false;
    }
  }
}

/* Recursively flush every vnode reachable from dentry. */
static void sync_dentry_tree(struct dentry_t *dentry, int depth) {
  if (dentry == NULL || depth > 32)
    return;

  struct vnode_t *vnode = dentry->vnode;
  if (vnode == NULL)
    return;

  /* Flush the real vnode (follow mounted_vnode if present). */
  struct vnode_t *effective = vnode->mounted_vnode ? vnode->mounted_vnode : vnode;
  sync_vnode(effective);

  /* Recurse into in-memory children. */
  list_for_each(&effective->children_dentries, pos) {
    struct dentry_t *child = container_of(pos, struct dentry_t, sibling_dentry);
    sync_dentry_tree(child, depth + 1);
  }
}

void vfs_sync_all(void) {
  debugk("vfs: syncing all dirty pages to disk...\n");

  /* Walk every mounted filesystem's dentry tree. */
  list_for_each(&mount_list, pos) {
    struct mount_t *mount = container_of(pos, struct mount_t, sibling_mount);
    if (mount->superblock == NULL || mount->superblock->root_dentry == NULL)
      continue;
    sync_dentry_tree(mount->superblock->root_dentry, 0);
  }

  debugk("vfs: sync complete\n");
}
