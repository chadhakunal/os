#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/filesystem/vfs/vfs.h"

DEFINE_SYSCALL0(sync)
{
  vfs_sync_all();
  return 0;
}
