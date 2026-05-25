#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/task/schedule.h"
#include "kernel/user_data_access.h"
#include "errno.h"
#include "types.h"

/* Implements rt_sigtimedwait — used by sigwait(3) with a NULL timeout.
   Waits until any signal in `set` is pending, consumes it, returns the signo. */
DEFINE_SYSCALL4(rt_sigtimedwait, const sigset_t *, user_set, void *, user_info,
                const void *, user_timeout, size_t, sigsetsize)
{
  if (sigsetsize != sizeof(sigset_t))
    return -EINVAL;

  sigset_t set;
  if (copy_from_user(&set, user_set, sizeof(sigset_t)) != 0)
    return -EFAULT;

  /* sigwait waits for signals in set regardless of whether they are blocked */
  while (1) {
    sigset_t pending = current_task->signal_state.pending & set;
    if (pending) {
      for (int sig = 1; sig < NUM_SIGS; sig++) {
        if (sig_in_set(&pending, sig)) {
          delete_signal_from_set(&current_task->signal_state.pending, sig);
          return sig;
        }
      }
    }

    /* No matching signal yet — block until something arrives.
     * Ignore task_block's return value: we want to loop and consume
     * the signal ourselves rather than return -EINTR. */
    task_block(WAIT_SIGNAL);
  }
}
