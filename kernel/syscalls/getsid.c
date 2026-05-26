#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "errno.h"

DEFINE_SYSCALL1(getsid, int64_t, pid)
{
  (void)pid;
  return (int64_t)current_task->pgid;
}
