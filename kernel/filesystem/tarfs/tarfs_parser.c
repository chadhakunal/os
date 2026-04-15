#include "kernel/filesystem/tarfs/tarfs_parser.h"
#include "kernel/filesystem/tarfs/tarfs.h"
#include "kernel/filesystem/vfs.h"
#include "kernel/filesystem/mode.h"
#include "types.h"
#include "lib/string.h"
#include "lib/printk/printk.h"

#define READ_EXECUTE_PERM PERM_RUSR | PERM_XUSR | PERM_RGRP | PERM_XGRP | PERM_ROTH | PERM_XOTH

struct dentry_t *search_children(const char *name, struct dentry_t *first_dentry) {
  if (first_dentry == NULL) return NULL;

  struct dentry_t *curr_dentry = first_dentry;
  do {
    if (strncmp(curr_dentry->name, name) == 0) {
      return curr_dentry;
    }
    curr_dentry = curr_dentry->sibling_dentry;
  } while (curr_dentry != first_dentry);

  return NULL;
}

void walk_and_create_path(const char *path, void *data, struct vnode_t *root_vnode, struct dentry_t *root_dentry, struct tar_header *header, uint32_t *last_id) {
  struct vnode_t *curr_vnode = root_vnode;
  struct dentry_t *curr_dentry = root_dentry;
  const char *current_path = path;
  char current_name[256];
  int name_len = str_tok(&current_path, current_name, '/', 256);
  // The first read will be '/' for the root path
  while (name_len > 0) {
    name_len = str_tok(&current_path, current_name, '/', 256);
    if (current_name[name_len-1] == '/' || tar_is_dir(header)) {
      printk("parsing, directory name: %s\n", current_name);
      // This is a directory!
      struct dentry_t *new_dentry = search_children(current_name, curr_vnode->first_child_dentry);
      if (new_dentry == NULL) {
        new_dentry = dentry_t_alloc();
        strncpy(new_dentry->name, current_name, 256);
        new_dentry->parent = curr_dentry;
        if (curr_vnode->first_child_dentry == NULL) {
          curr_vnode->first_child_dentry = new_dentry;
          curr_vnode->last_child_dentry = new_dentry;
          new_dentry->sibling_dentry = new_dentry;
        } else {
          new_dentry->sibling_dentry = curr_vnode->first_child_dentry;
          curr_vnode->last_child_dentry->sibling_dentry = new_dentry;
        }
        // If there is no dentry, there is also no inode!
        struct vnode_t *new_vnode = vnode_t_alloc();
        new_vnode->size = 0;
        new_vnode->id = *last_id + 1;
        *last_id += 1;
        new_vnode->refcount = 1;
        new_vnode->owner_uid = 0;
        new_vnode->owner_gid = 0;
        new_vnode->permission_mode = READ_EXECUTE_PERM | PERM_IS_DIR;
        new_dentry->vnode = new_vnode;
        new_vnode->first_child_dentry = NULL;
        new_vnode->last_child_dentry = NULL;
        new_vnode->fs_private_vnode = NULL;
      }
      curr_vnode = new_dentry->vnode;
      curr_dentry = new_dentry;
    } else {
      // This is the actual file
      struct dentry_t *new_dentry = search_children(current_name, curr_vnode->first_child_dentry);
      if (new_dentry == NULL) {
        new_dentry = dentry_t_alloc();
        strncpy(new_dentry->name, current_name, 256);
        new_dentry->parent = curr_dentry;
        if (curr_vnode->first_child_dentry == NULL) {
          curr_vnode->first_child_dentry = new_dentry;
          curr_vnode->last_child_dentry = new_dentry;
          new_dentry->sibling_dentry = new_dentry;
        } else {
          new_dentry->sibling_dentry = curr_vnode->first_child_dentry;
          curr_vnode->last_child_dentry->sibling_dentry = new_dentry;
        }
      }
      if (new_dentry->vnode == NULL) {
        struct vnode_t *new_vnode = vnode_t_alloc();
        new_dentry->vnode = new_vnode;
        new_vnode->size = parse_octal(header->size, 12);
        new_vnode->id = *last_id + 1;
        *last_id += 1;
        new_vnode->refcount = 1;
        new_vnode->owner_uid = 0;
        new_vnode->owner_gid = 0;
        new_vnode->permission_mode = READ_EXECUTE_PERM;
        struct tarfs_vnode_t *tarfs_vnode = tarfs_vnode_t_alloc();
        tarfs_vnode->data = data;
        new_vnode->fs_private_vnode = (void *)tarfs_vnode;
      }
      return;
    }
  }
}

struct vnode_t *parse_tar(void *data, uint64_t tar_size, struct superblock_t *sb) {
  uint8_t *tar_ptr = (uint8_t *)data;
  uint8_t *tar_end = tar_ptr + tar_size;
  uint32_t last_id = 0;

  struct vnode_t *root_vnode = vnode_t_alloc();
  struct tarfs_vnode_t *root_tarfs_vnode = tarfs_vnode_t_alloc();
  root_vnode->size = 0;
  root_vnode->id = 0;
  root_vnode->refcount = 1;
  root_vnode->owner_uid = 0;
  root_vnode->owner_gid = 0;
  root_vnode->permission_mode = READ_EXECUTE_PERM | PERM_IS_DIR;
  root_vnode->superblock = sb;
  root_vnode->first_child_dentry = NULL;
  root_vnode->last_child_dentry = NULL;
  root_vnode->fs_private_vnode = (void *)root_tarfs_vnode;
  printk("SETTING root vnode: %lld\n", root_vnode->permission_mode);

  struct dentry_t *root_dentry = dentry_t_alloc();
  strncpy(root_dentry->name, "/", 256);
  root_dentry->vnode = root_vnode;
  root_dentry->parent = root_dentry;
  root_dentry->sibling_dentry = NULL;
  sb->root_dentry = root_dentry;
  sb->root_vnode = root_vnode;

  while (tar_ptr < tar_end) {
    struct tar_header *header = (struct tar_header *)tar_ptr;

    if (header->name[0] == '\0') break;

    uint64_t file_size = parse_octal(header->size, 12);
    void *file_data = (void *)(tar_ptr + 512);

    walk_and_create_path(header->name, file_data, root_vnode, root_dentry, header, &last_id);

    uint64_t data_blocks = (file_size + 511) / 512;
    tar_ptr += 512 + (data_blocks * 512);
  }

  return root_vnode;
}
