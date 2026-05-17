#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "types.h"

DEFINE_SYSCALL2(rt_sigpending, sigset_t *, user_set, size_t, sigsetsize)
{
  if (sigsetsize != sizeof(sigset_t))
    return -1;

  sigset_t pending = current_task->signal_state.pending &
                     current_task->signal_state.blocked;

  if (copy_to_user(user_set, &pending, sizeof(sigset_t)) != 0)
    return -1;

  return 0;
}
