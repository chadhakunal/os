#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/user_data_access.h"
#include "lib/list.h"
#include "types.h"

static bool dentry_is_child_of_dir(struct vnode_t *dir, struct dentry_t *d) {
  list_for_each(&dir->children_dentries, pos) {
    struct dentry_t *ch = container_of(pos, struct dentry_t, sibling_dentry);
    if (ch == d) {
      return true;
    }
  }
  return false;
}

struct kernel_dirent {
  uint32_t      d_ino;
  unsigned char d_type;
  char          d_name[256];
};

/* getdents(fd, buf, count) — fills up to count entries into buf.
   Returns number of entries written, or -1 on error. */
DEFINE_SYSCALL3(getdents, int, fd, struct kernel_dirent *, buf, uint32_t, count) {
  struct file_t *file = find_file(&current_task->file_table, fd);
  if (file == NULL)
    return -1;

  struct vnode_t *vnode = file->vnode;
  if (vnode == NULL)
    return -1;

  uint32_t filled = 0;
  while (filled < count) {
    struct dentry_t *dentry;
    int64_t ret = vfs_readdir(vnode, file->offset + filled, &dentry);
    if (ret < 0 || dentry == NULL)
      break;

    struct kernel_dirent kd;
    kd.d_ino = dentry->vnode != NULL ? dentry->vnode->id : dentry->readdir_ino;
    uint32_t mode = dentry->vnode != NULL ? dentry->vnode->permission_mode : 0;
    uint32_t fmt = mode & 0170000;
    if      (fmt == 0040000) kd.d_type = 4;  /* DT_DIR */
    else if (fmt == 0100000) kd.d_type = 8;  /* DT_REG */
    else if (fmt == 0120000) kd.d_type = 10; /* DT_LNK */
    else                     kd.d_type = 0;  /* DT_UNKNOWN */
    copy_to_user(&buf[filled].d_ino,  &kd.d_ino,  sizeof(kd.d_ino));
    copy_to_user(&buf[filled].d_type, &kd.d_type, sizeof(kd.d_type));
    copy_string_to_user(buf[filled].d_name, dentry->name, 256);

    filled++;

    /*
     * Dentries not in the parent's children list are ephemeral (returned by
     * the filesystem's readdir without being cached). Free them after the copy.
     * tarfs and devfs pre-build their trees so their dentries are always cached;
     * sbfs_readdir returns an ephemeral dentry with vnode == NULL each call.
     */
    if (!dentry_is_child_of_dir(vnode, dentry)) {
      dentry_t_free(dentry);
    }
  }

  file->offset += filled;
  return filled;
}
