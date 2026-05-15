#define DEBUG 0
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
  debugk("waitpid: PID %llu waiting for child pid=%lld\n", current_task->pid, pid);

  if (options != 0) {
    return -EINVAL;
  }

  if (pid == 0 || pid < -1) {
    return -EINVAL;
  }

  while (1) {
    debugk("waitpid: PID %llu checking for zombie child\n", current_task->pid);
    struct task_t *zombie = find_zombie_child(current_task, pid);

    if (zombie) {
      debugk("waitpid: PID %llu found zombie child PID %llu\n", current_task->pid, zombie->pid);
      if (wstatus) {
        copy_to_user(wstatus, &zombie->exit_status, sizeof(int));
      }
      uint64_t child_pid = zombie->pid;
      debugk("waitpid: PID %llu about to reap zombie PID %llu\n", current_task->pid, child_pid);
      reap_zombie(zombie);
      debugk("waitpid: PID %llu reaped zombie, returning child_pid=%llu\n", current_task->pid, child_pid);
      return child_pid;
    }

    debugk("waitpid: PID %llu no zombie found, checking for alive children\n", current_task->pid);
    if (!has_alive_children(current_task, pid)) {
      debugk("waitpid: PID %llu has no alive children, returning -ECHILD\n", current_task->pid);
      return -ECHILD;
    }

    debugk("waitpid: PID %llu blocking to wait for child\n", current_task->pid);
    current_task->wait_reason = WAIT_CHILD;
    current_task->wait_pid = pid;
    current_task->state = TASK_BLOCKED;
    schedule();

    debugk("waitpid: PID %llu woke up from wait\n", current_task->pid);
    asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));
  }
}
