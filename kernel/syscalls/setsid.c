#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "errno.h"

DEFINE_SYSCALL0(setsid)
{
  /* Process group leaders cannot become session leaders. */
  if (current_task->pid == current_task->pgid)
    return -EPERM;

  current_task->sid  = current_task->pid;
  current_task->pgid = current_task->pid;
  return (int64_t)current_task->sid;
}
