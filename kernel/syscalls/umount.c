#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "lib/list.h"
#include "errno.h"

int64_t sys_umount2(struct trap_frame *tf) {
  const char *u_target = (const char *)tf->a0;

  char target[512];
  copy_string_from_user(target, u_target, sizeof(target));

  /* Find the mount entry for this path. */
  struct mount_t *found = NULL;
  list_for_each(&mount_list, pos) {
    struct mount_t *m = container_of(pos, struct mount_t, sibling_mount);
    if (strncmp(m->root_path, target) == 0) {
      found = m;
      break;
    }
  }
  if (found == NULL)
    return -EINVAL;

  /* Flush all dirty pages before detaching. */
  vfs_sync_all();

  /* Resolve the mount point dentry and clear its mounted_vnode. */
  struct dentry_t *dentry = NULL;
  if (vfs_resolve_path(target, &dentry) == 0 && dentry != NULL)
    dentry->vnode->mounted_vnode = NULL;

  /* Remove from mount list. */
  list_remove(&found->sibling_mount);

  printk("umount: %s unmounted\n", target);
  return 0;
}
