#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "lib/printk/printk.h"

DEFINE_SYSCALL2(kill, int, pid, int, sig)
{
  debugk("syscall: kill(pid=%d, sig=%d) from PID %llu\n", pid, sig, current_task->pid);

  if (sig < 0 || sig >= NUM_SIGS) {
    debugk("kill: invalid signal %d\n", sig);
    return -1;
  }

  if (pid < 0) {
    /* Negative pid: send to process group -pid */
    uint64_t pgid = (uint64_t)(-pid);
    send_signal_to_pgid(pgid, sig);
    debugk("kill: sent signal %d to pgid %llu\n", sig, pgid);
    return 0;
  }

  if (pid == 0) {
    /* pid == 0: send to own process group */
    send_signal_to_pgid(current_task->pgid, sig);
    debugk("kill: sent signal %d to own pgid %llu\n", sig, current_task->pgid);
    return 0;
  }

  struct task_t *target = find_task_by_pid((uint64_t)pid);
  if (!target) {
    debugk("kill: process %d not found\n", pid);
    return -1;
  }

  send_signal(target, sig);
  debugk("kill: sent signal %d to PID %d\n", sig, pid);

  return 0;
}
