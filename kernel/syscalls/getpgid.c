#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"

DEFINE_SYSCALL1(getpgid, int64_t, pid)
{
  if (pid == 0)
    return (int64_t)current_task->pgid;

  struct task_t *task = find_task_by_pid((uint64_t)pid);
  if (!task)
    return -1;

  return (int64_t)task->pgid;
}
