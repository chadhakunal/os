#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "errno.h"

DEFINE_SYSCALL1(getsid, int64_t, pid)
{
  if (pid == 0)
    return (int64_t)current_task->sid;

  struct task_t *task = find_task_by_pid((uint64_t)pid);
  if (!task)
    return -ESRCH;

  return (int64_t)task->sid;
}
