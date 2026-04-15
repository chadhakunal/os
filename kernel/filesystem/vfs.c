#include "kernel/filesystem/vfs.h"
#include "kernel/filesystem/tarfs/tarfs.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "panic.h"
#include "kernel/filesystem/mode.h"

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
  base_mount->superblock = tarfs_mount((void *) _tarfs_start, (uint64_t) _tarfs_size);
  base_mount->next_mount = base_mount;
}

// struct mount_t *longest_prefix_mount(const char *path, char **leftover_path) {
//   // Note: leftover_path will be whatever didn't match, and will not start with '/'!
//   mount_t *best     = base_mount;
//   size_t   best_len = 1;
//   *leftover_path = (char *) path + best_len;
//
//   mount_t *m = base_mount->next_mount;
//   while (m != base_mount) {
//     size_t len = strlen(m->path);
//     if (strncmp(m->path, path, len) == 0 && len > best_len) {
//       best     = m;
//       best_len = len;
//       *leftover_path = (char *) path + best_len;
//     }
//     m = m->next;
//   }
//   return best;
// }

int32_t vfs_resolve_path(const char *path, struct dentry_t **out) {
  struct dentry_t *curr_dentry = base_mount->superblock->root_dentry;
  struct dentry_t *next_dentry;
  uint32_t ret;
  const char *current_path = path;
  char current_name[256];
  int name_len = str_tok_no_delim(&current_path, current_name, '/', 256); // Extracts "/" from current_path and sets current_name to ""
  name_len = str_tok_no_delim(&current_path, current_name, '/', 256); 
  while (name_len > 0) {
    ret = vfs_lookup(current_name, curr_dentry, &next_dentry);
    if (ret != 0 || next_dentry == NULL) {
      return ret;
    }
    curr_dentry = next_dentry;
    name_len = str_tok_no_delim(&current_path, current_name, '/', 256);
  }
  *out = curr_dentry;
  return 0;
}

int32_t vfs_lookup(const char *name, struct dentry_t *parent_dir, struct dentry_t **out) {
  printk("vfs_lookup: Looking up %s\n", name);
  if (parent_dir == NULL) {
    panic("vfs_lookup: parent_dir is NULL\n");
  }
  printk("vfs_lookup: parent_dir name: %s\n", parent_dir->name);
  printk("vfs_lookup: parent_dir permisson_bits: %lld, is_dir: %lld, all_perms: %lld\n", parent_dir->vnode->permission_mode, PERM_IS_DIR, READ_EXECUTE_PERM | PERM_IS_DIR);

  if (!IS_DIR(parent_dir->vnode->permission_mode)) {
    panic("vfs_lookup: parent_dir is not a directory\n");
  }

  /*
  * Note: This is pre- vfs abstraction, at this moment tarfs is only being used
  * Thus everything will be in memory, so tarfs has no fs specific handlers. This
  * needs to be abstracted to allow for other fs to hook into it, ie inode->i_ops->lookup()
  * First search for the dentry in the in memory dcache (should have negative dentries, and positive dentries)
  * if not found as neg dentry or pos dentry must ask underlying fs via method above
  */

  struct dentry_t *child_dentry = parent_dir->vnode->first_child_dentry;
  if (child_dentry == NULL) {
    return -1;
  }
  struct dentry_t *dentry = child_dentry;
  do {
    if (strncmp(dentry->name, name) == 0) {
      *out = dentry;
      return 0;
    }
  } while (child_dentry != dentry);
  *out = NULL;
  return -1;
}
