#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "lib/printk/printk.h"
#include "types.h"

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

DEFINE_SYSCALL4(rt_sigprocmask, int, how, const sigset_t *, user_set,
                sigset_t *, user_oldset, size_t, sigsetsize)
{
  if (sigsetsize != sizeof(sigset_t))
    return -1;

  if (user_oldset != NULL) {
    sigset_t old = current_task->signal_state.blocked;
    if (copy_to_user(user_oldset, &old, sizeof(sigset_t)) != 0)
      return -1;
  }

  if (user_set != NULL) {
    sigset_t set;
    if (copy_from_user(&set, user_set, sizeof(sigset_t)) != 0)
      return -1;

    /* SIGKILL and SIGSTOP cannot be blocked */
    set &= ~((1ULL << (SIGKILL - 1)) | (1ULL << (SIGSTOP - 1)));

    switch (how) {
      case SIG_BLOCK:
        current_task->signal_state.blocked |= set;
        break;
      case SIG_UNBLOCK:
        current_task->signal_state.blocked &= ~set;
        break;
      case SIG_SETMASK:
        current_task->signal_state.blocked = set;
        break;
      default:
        return -1;
    }
  }

  return 0;
}
