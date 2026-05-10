#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "lib/printk/printk.h"

DEFINE_SYSCALL2(kill, int, pid, int, sig)
{
  debugk("syscall: kill(pid=%d, sig=%d) from PID %llu\n", pid, sig, current_task->pid);

  // Validate signal number
  if (sig < 0 || sig >= NUM_SIGS) {
    debugk("kill: invalid signal %d\n", sig);
    return -1;
  }

  // Find target process
  struct task_t *target = find_task_by_pid((uint64_t)pid);
  if (!target) {
    debugk("kill: process %d not found\n", pid);
    return -1;
  }

  // Add signal to target's pending set
  add_signal_to_set(&target->signal_state.pending, sig);
  debugk("kill: added signal %d to PID %d pending set\n", sig, pid);

  return 0;
}
