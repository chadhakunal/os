#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/task/schedule.h"
#include "kernel/time/timer.h"
#include "kernel/user_data_access.h"
#include "errno.h"
#include "types.h"

struct timespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

/* Minimal siginfo written back to userspace when info != NULL. */
struct kernel_siginfo {
  int si_signo;
  int si_errno;
  int si_code;
  int si_pid;
  int si_uid;
};

#define TICKS_PER_SEC (10000000ULL / TIMER_INTERVAL_CYCLES)
#define SI_USER 0

DEFINE_SYSCALL4(rt_sigtimedwait, const sigset_t *, user_set, void *, user_info,
                const void *, user_timeout, size_t, sigsetsize)
{
  if (sigsetsize != sizeof(sigset_t))
    return -EINVAL;

  sigset_t set;
  if (copy_from_user(&set, user_set, sizeof(sigset_t)) != 0)
    return -EFAULT;

  /* Parse timeout if provided. */
  uint64_t deadline = 0;
  int has_timeout = 0;
  if (user_timeout != NULL) {
    struct timespec ts;
    if (copy_from_user(&ts, user_timeout, sizeof(ts)) != 0)
      return -EFAULT;
    if (ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000LL)
      return -EINVAL;
    uint64_t ticks = (uint64_t)ts.tv_sec * TICKS_PER_SEC
                   + (uint64_t)ts.tv_nsec * TICKS_PER_SEC / 1000000000ULL;
    deadline = virtual_time.os_ticks + ticks;
    has_timeout = 1;
  }

  sigset_t pending;
  while (1) {
    if (has_timeout && virtual_time.os_ticks >= deadline)
      return -EAGAIN;

    pending = current_task->signal_state.pending & set;
    if (pending) {
      for (int sig = 1; sig < NUM_SIGS; sig++) {
        if (sig_in_set(&pending, sig)) {
          delete_signal_from_set(&current_task->signal_state.pending, sig);

          if (user_info != NULL) {
            struct kernel_siginfo info = {0};
            info.si_signo = sig;
            info.si_errno = 0;
            info.si_code  = SI_USER;
            info.si_pid   = (int)current_task->ppid;
            info.si_uid   = 0;
            copy_to_user(user_info, &info, sizeof(info));
          }

          return sig;
        }
      }
    }

    /* Block until a signal arrives or the deadline passes.
     * With a timeout, use WAIT_SLEEP so the timer wakes us at deadline.
     * Without a timeout, use WAIT_SIGNAL so any signal wakes us. */
    current_task->restart_blocked = 1;
    if (has_timeout) {
      current_task->sleep_until = deadline;
      task_block(WAIT_SLEEP);
    } else {
      task_block(WAIT_SIGNAL);
    }
  }
}
