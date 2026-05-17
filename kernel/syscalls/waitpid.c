#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "lib/printk/printk.h"
#include "types.h"
#include "errno.h"


DEFINE_SYSCALL3(waitpid, int64_t, pid, int *, wstatus, int, options)
{
  debugk("waitpid: PID %llu waiting for child pid=%lld\n", current_task->pid, pid);

  if (options != 0)
    return -EINVAL;

  /* pid > 0 : specific child pid
     pid == 0 : any child in caller's pgid
     pid == -1: any child
     pid < -1 : any child in pgid == -pid */
  int64_t  specific_pid = -1;
  uint64_t pgid         = 0;

  if (pid > 0)
    specific_pid = pid;
  else if (pid == 0)
    pgid = current_task->pgid;
  else if (pid < -1)
    pgid = (uint64_t)(-pid);

  while (1) {
    debugk("waitpid: PID %llu checking for zombie child (specific=%lld pgid=%llu)\n",
           current_task->pid, specific_pid, pgid);
    struct task_t *zombie = find_zombie_child(current_task, specific_pid, pgid);

    if (zombie) {
      if (wstatus)
        copy_to_user(wstatus, &zombie->exit_status, sizeof(int));
      uint64_t child_pid = zombie->pid;
      reap_zombie(zombie);
      return child_pid;
    }

    if (!has_alive_children(current_task, specific_pid, pgid)) {
      debugk("waitpid: PID %llu has no matching children, returning -ECHILD\n", current_task->pid);
      return -ECHILD;
    }

    current_task->wait_reason = WAIT_CHILD;
    current_task->wait_pid    = specific_pid;
    current_task->state       = TASK_BLOCKED;
    schedule();

    asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));

    /* SIGCHLD wakes waitpid but doesn't interrupt it — clear it and loop. */
    current_task->signal_state.pending &= ~(1ULL << (SIGCHLD - 1));
    sigset_t pending_unblocked = current_task->signal_state.pending
                                 & ~current_task->signal_state.blocked;
    if (pending_unblocked)
      return -EINTR;
  }
}
