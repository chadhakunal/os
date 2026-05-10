#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/trap.h"
#include "kernel/printk.h"

int64_t sys_rt_sigreturn(struct trap_frame *tf) {
  return -1;
}
