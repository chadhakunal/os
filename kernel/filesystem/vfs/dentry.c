#include "kernel/filesystem/vfs/vfs.h"
#include "lib/printk/printk.h"

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
