#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "lib/printk/printk.h"
#include "types.h"

DEFINE_SYSCALL3(waitpid, int64_t, pid, int *, wstatus, int, options)
{
  // Only support blocking wait for now (options == 0)
  if (options != 0) {
    return -1;  // EINVAL - options not supported yet
  }

  if (pid == 0 || pid < -1) {
    return -1;  // EINVAL - process groups not supported yet
  }

  while (1) {
    struct task_t *zombie = find_zombie_child(current_task, pid);

    if (zombie) {
      if (wstatus) {
        *wstatus = zombie->exit_status;
      }
      uint64_t child_pid = zombie->pid;
      reap_zombie(zombie);
      return child_pid;
    }

    if (!has_alive_children(current_task, pid)) {
      return -1;  // ECHILD - no such child exists
    }

    printk("waitpid: PID %llu blocking, waiting for child %lld\n",
           current_task->pid, pid);
    current_task->wait_reason = WAIT_CHILD;
    current_task->wait_pid = pid;
    current_task->state = TASK_BLOCKED;
    schedule();
  }
}
