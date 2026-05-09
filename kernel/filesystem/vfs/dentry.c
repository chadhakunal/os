#include "kernel/filesystem/vfs/vfs.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "errno.h"

#define MAX_DEPTH_SIZE 32

void vfs_print_dentry(struct dentry_t *dentry) {
  if (dentry == NULL) {
    printk("[dentry: NULL]\n");
    return;
  }
  printk("[dentry name=\"%s\", vnode=%p, parent=%p, sibling=%p]\n",
         dentry->name,
         dentry->vnode,
         dentry->parent,
         dentry->sibling_dentry);
}

int64_t vfs_dentry_get_path(struct dentry_t *dentry, char *buf, size_t size) {
  struct dentry_t *dentry_stack[MAX_DEPTH_SIZE];
  int dentry_stack_pos = 0;
  size_t path_pos = 0;
  struct dentry_t *curr_dentry;

  if (dentry->parent == NULL || dentry->parent == dentry) {
    if (size < 2) {
      return -ERANGE;
    }
    buf[0] = '/';
    buf[1] = '\0';
    return 1;
  }

  curr_dentry = dentry;
  while (curr_dentry != NULL && curr_dentry->parent != NULL && curr_dentry->parent != curr_dentry) {
    if (dentry_stack_pos >= MAX_DEPTH_SIZE) {
      return -ERANGE;
    }
    dentry_stack[dentry_stack_pos++] = curr_dentry;
    curr_dentry = curr_dentry->parent;
  }

  for (int i = dentry_stack_pos - 1; i >= 0; i--) {
    curr_dentry = dentry_stack[i];
    size_t dentry_name_len = str_len(curr_dentry->name, MAX_PATH_NAME_LEN);

    if (path_pos + 1 + dentry_name_len + 1 > size) {
      return -ERANGE;
    }

    buf[path_pos++] = '/';
    for (size_t j = 0; j < dentry_name_len; j++) {
      buf[path_pos++] = curr_dentry->name[j];
    }
  }

  buf[path_pos] = '\0';

  return path_pos;
}
