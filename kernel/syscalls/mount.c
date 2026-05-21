#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/sbfs/sbfs.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "errno.h"

/*
 * mount(source, target, fstype, flags, data)
 *
 * Only supported filesystem: "sbfs" (the block-device-backed writable store).
 * source and data are ignored.  flags are ignored (always read-write).
 *
 * This lets userspace (rc script) call mount("", "/mnt", "sbfs", 0, NULL)
 * to explicitly wire the writable filesystem at the desired path.
 */
int64_t sys_mount(struct trap_frame *tf) {
  const char *u_target = (const char *)tf->a1;
  const char *u_fstype = (const char *)tf->a2;

  char target[512];
  char fstype[32];

  copy_string_from_user(target, u_target, sizeof(target));
  copy_string_from_user(fstype, u_fstype, sizeof(fstype));

  if (strncmp(fstype, "sbfs") != 0)
    return -ENODEV;

  struct superblock_t *sb = vfs_get_sbfs();
  if (sb == NULL)
    return -ENODEV;

  int32_t ret = vfs_mount_at(target, sb);
  if (ret < 0)
    return ret;

  printk("mount: sbfs mounted at %s\n", target);
  return 0;
}
