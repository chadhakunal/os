#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "types.h"

DEFINE_SYSCALL0(fork) 
{
  return fork_off();
}
