#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"
#include "types.h"

DEFINE_SYSCALL1(exit, int, status)
{
  debugk("exit: PID %llu exiting with status %d\n", current_task->pid, status);

  current_task->exit_status = status;

  close_all_files(current_task);
  clear_vmas(current_task);

  current_task->state = TASK_ZOMBIE;

  struct task_t *parent = find_task_by_pid(current_task->ppid);

  if (parent && parent->state == TASK_BLOCKED && parent->wait_reason == WAIT_CHILD) {
    // Check if parent is waiting for us specifically or any child
    if (parent->wait_pid == -1 || parent->wait_pid == (int64_t)current_task->pid) {
      debugk("exit: Waking parent PID %llu\n", parent->pid);
      unblock_task(parent);
    }
  }

  schedule();

  // Should never reach here
  panic("exit: returned from schedule()!");
  return 0;
}
