#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/trap.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/task/schedule.h"
#include "kernel/user_data_access.h"
#include "lib/printk/printk.h"
#include "errno.h"
#include "types.h"

DEFINE_SYSCALL2(rt_sigsuspend, const sigset_t *, user_mask, size_t, sigsetsize)
{
  if (sigsetsize != sizeof(sigset_t))
    return -EINVAL;

  sigset_t mask;
  if (copy_from_user(&mask, user_mask, sizeof(sigset_t)) != 0)
    return -EFAULT;

  /* SIGKILL and SIGSTOP cannot be blocked */
  mask &= ~((1ULL << (SIGKILL - 1)) | (1ULL << (SIGSTOP - 1)));

  sigset_t saved_blocked = current_task->signal_state.blocked;
  current_task->signal_state.blocked = mask;

  current_task->wait_reason = WAIT_SIGNAL;
  current_task->state = TASK_BLOCKED;
  schedule();

  asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));
  current_task->signal_state.blocked = saved_blocked;

  return -EINTR;
}
