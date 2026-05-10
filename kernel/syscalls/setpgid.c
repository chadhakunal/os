#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "errno.h"

DEFINE_SYSCALL2(setpgid, int, pid, int, pgid) {
  struct task_t *target;

  if (pid == 0) {
    target = current_task;
  } else {
    target = find_task_by_pid((uint64_t)pid);
    if (!target) {
      return -ESRCH;
    }
  }

  if (pgid < 0) {
    return -EINVAL;
  }

  if (pgid == 0) {
    target->pgid = target->pid;
  } else {
    target->pgid = (uint64_t)pgid;
  }

  return 0;
}
