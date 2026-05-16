#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/sbi.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "lib/printk/printk.h"

/* cmd values, mirroring Linux's RB_* constants */
#define RB_POWER_OFF  0
#define RB_AUTOBOOT   1  /* reboot */

DEFINE_SYSCALL1(reboot, int, cmd) {
  printk("kernel: syncing filesystems...\n");
  vfs_sync_all();

  if (cmd == RB_AUTOBOOT) {
    printk("kernel: rebooting...\n");
    sbi_system_reset(SBI_RESET_COLD, 0);
  } else {
    printk("kernel: shutting down...\n");
    sbi_system_reset(SBI_RESET_SHUTDOWN, 0);
  }

  /* Unreachable — sbi_system_reset spins forever on failure. */
  return 0;
}
