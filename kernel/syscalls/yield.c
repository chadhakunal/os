#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "types.h"
#include "kernel/task/schedule.h"
#include "kernel/task/task.h"

DEFINE_SYSCALL0(sched_yield)
{
  // Force expiration by setting runtime to max
  current_task->runtime = current_task->max_runtime;
  schedule();
  return 0;
}
