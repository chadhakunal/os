#include "kernel/filesystem/tarfs/tarfs_parser.h"
#include "kernel/filesystem/tarfs/tarfs.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/mode.h"
#include "types.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "lib/list.h"

#define READ_EXECUTE_PERM PERM_RUSR | PERM_XUSR | PERM_RGRP | PERM_XGRP | PERM_ROTH | PERM_XOTH

struct dentry_t *search_children(const char *name, struct list_node *dentry_head) {
  list_for_each(dentry_head, pos) {
    struct dentry_t *dentry = container_of(pos, struct dentry_t, sibling_dentry);
    if (strncmp(dentry->name, name) == 0) {
      return dentry;
    }
  }
  return NULL;
}

void walk_and_create_path(const char *path, void *data, struct vnode_t *root_vnode, struct dentry_t *root_dentry, struct tar_header *header) {
  struct vnode_t *curr_vnode = root_vnode;
  struct dentry_t *curr_dentry = root_dentry;
  const char *current_path = path;
  char current_name[256];
  int name_len = str_tok(&current_path, current_name, '/', 256);
  name_len = str_tok(&current_path, current_name, '/', 256);
  while (name_len > 0) {

    // Strip trailing '/' from the name for dentry storage
    bool is_dir_path = (current_name[name_len-1] == '/');
    if (is_dir_path) {
      current_name[name_len-1] = '\0';
    }

    if (is_dir_path || tar_is_dir(header)) {
      // This is a directory!
      struct dentry_t *new_dentry = search_children(current_name, &curr_vnode->children_dentries);
      if (new_dentry == NULL) {
        new_dentry = dentry_t_alloc();
        strncpy(new_dentry->name, current_name, 256);
        new_dentry->parent = curr_dentry;
        list_append(&curr_vnode->children_dentries, &new_dentry->sibling_dentry);
        // If there is no dentry, there is also no inode!
        struct vnode_t *new_vnode = tarfs_alloc_vnode(root_vnode->superblock);
        new_vnode->permission_mode = READ_EXECUTE_PERM | S_IFDIR;
        new_dentry->vnode = new_vnode;
      }
      curr_vnode = new_dentry->vnode;
      curr_dentry = new_dentry;
    } else {
      // This is the actual file
      struct dentry_t *new_dentry = search_children(current_name, &curr_vnode->children_dentries);
      if (new_dentry == NULL) {
        new_dentry = dentry_t_alloc();
        strncpy(new_dentry->name, current_name, 256);
        new_dentry->parent = curr_dentry;
        list_append(&curr_vnode->children_dentries, &new_dentry->sibling_dentry);
        struct vnode_t *new_vnode = tarfs_alloc_vnode(root_vnode->superblock);
        uint64_t file_size = parse_octal(header->size, 12);
        new_vnode->permission_mode = READ_EXECUTE_PERM | S_IFREG;
        new_vnode->size = file_size;
        new_dentry->vnode = new_vnode;
        struct tarfs_vnode_t *tarfs_vnode = (tarfs_vnode_t *)new_vnode->fs_private_data;
        tarfs_vnode->data = data;
      }
      return;
    }
    name_len = str_tok(&current_path, current_name, '/', 256);
  }
}

struct vnode_t *parse_tar(void *data, uint64_t tar_size, struct superblock_t *sb) {
  uint8_t *tar_ptr = (uint8_t *)data;
  uint8_t *tar_end = tar_ptr + tar_size;

  struct vnode_t *root_vnode = tarfs_alloc_vnode(sb);
  root_vnode->permission_mode = READ_EXECUTE_PERM | S_IFDIR;
  struct tarfs_vnode_t *root_tarfs_vnode = (tarfs_vnode_t *)root_vnode->fs_private_data;

  struct dentry_t *root_dentry = dentry_t_alloc();
  strncpy(root_dentry->name, "/", 256);
  root_dentry->vnode = root_vnode;
  root_dentry->parent = root_dentry;
  sb->root_dentry = root_dentry;
  sb->root_vnode = root_vnode;

  while (tar_ptr < tar_end) {
    struct tar_header *header = (struct tar_header *)tar_ptr;

    if (header->name[0] == '\0') break;

    uint64_t file_size = parse_octal(header->size, 12);
    void *file_data = (void *)(tar_ptr + 512);

    walk_and_create_path(header->name, file_data, root_vnode, root_dentry, header);

    uint64_t data_blocks = (file_size + 511) / 512;
    tar_ptr += 512 + (data_blocks * 512);
  }

  return root_vnode;
}
