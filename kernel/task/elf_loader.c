#include "kernel/task/elf_loader.h"
#include "lib/printk/printk.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "panic.h"

void load_elf(struct task_t *task, const char *path) {
  struct dentry_t *dentry;
  int64_t ret = vfs_resolve_path(path, &dentry);
  if (ret != 0) {
    panic("load_elf: vfs_resolve_path returned with error\n");
    return;
  }

  // Go through 
  struct Elf64_Ehdr header;
  vfs_vnode_read(dentry->vnode, &header, sizeof(header), 0);
}
