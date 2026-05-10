#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/user_data_access.h"
#include "lib/printk/printk.h"
#include "types.h"

#define EINVAL 22
#define ECHILD 10

DEFINE_SYSCALL3(waitpid, int64_t, pid, int *, wstatus, int, options)
{
  if (options != 0) {
    return -EINVAL;
  }

  if (pid == 0 || pid < -1) {
    return -EINVAL;
  }

  while (1) {
    struct task_t *zombie = find_zombie_child(current_task, pid);

    if (zombie) {
      if (wstatus) {
        copy_to_user(wstatus, &zombie->exit_status, sizeof(int));
      }
      uint64_t child_pid = zombie->pid;
      reap_zombie(zombie);
      return child_pid;
    }

    if (!has_alive_children(current_task, pid)) {
      return -ECHILD;
    }

    current_task->wait_reason = WAIT_CHILD;
    current_task->wait_pid = pid;
    current_task->state = TASK_BLOCKED;
    schedule();
  }
}
