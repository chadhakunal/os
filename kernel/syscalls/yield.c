#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "types.h"

DEFINE_SYSCALL0(sched_yield)
{
  return 0;
}
