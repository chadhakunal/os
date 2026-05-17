#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"

/* umask(mask) — set creation mask; return previous mask. */
DEFINE_SYSCALL1(umask, uint32_t, mask)
{
  uint32_t old = current_task->umask;
  current_task->umask = mask & 0777u;
  return (int64_t)old;
}
