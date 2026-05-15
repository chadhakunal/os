#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "lib/printk/printk.h"
#include "types.h"

DEFINE_SYSCALL4(rt_sigaction, int, signum, const struct sigaction_t *, user_act,
                struct sigaction_t *, user_oldact, size_t, sigsetsize)
{
  debugk("rt_sigaction: signum=%d, act=%p, oldact=%p, sigsetsize=%lu\n",
         signum, user_act, user_oldact, sigsetsize);

  // Validate signal number
  if (signum < 1 || signum >= NUM_SIGS) {
    debugk("rt_sigaction: invalid signal number %d\n", signum);
    return -1;
  }

  // SIGKILL and SIGSTOP cannot be caught or ignored
  if (signum == SIGKILL || signum == SIGSTOP) {
    debugk("rt_sigaction: cannot set handler for SIGKILL or SIGSTOP\n");
    return -1;
  }

  // Validate sigsetsize (should be sizeof(sigset_t))
  if (sigsetsize != sizeof(sigset_t)) {
    debugk("rt_sigaction: invalid sigsetsize %lu (expected %lu)\n",
           sigsetsize, sizeof(sigset_t));
    return -1;
  }

  struct sigaction_t *old_action = current_task->signal_state.actions[signum];

  // Return old action if requested
  if (user_oldact != NULL) {
    if (old_action != NULL &&
        old_action != (struct sigaction_t *)SIG_DEFAULT_HANDLER &&
        old_action != (struct sigaction_t *)SIG_IGNORE) {
      // Copy kernel sigaction to user space
      if (copy_to_user(user_oldact, old_action, sizeof(struct sigaction_t)) != 0) {
        debugk("rt_sigaction: failed to copy old action to user\n");
        return -1;
      }
    } else {
      // For SIG_DFL or SIG_IGN, create a temporary structure
      struct sigaction_t temp_action;
      temp_action.sa_handler = (void (*)(int))old_action;
      temp_action.sa_mask = 0;
      temp_action.sa_flags = 0;
      temp_action.sa_restorer = NULL;

      if (copy_to_user(user_oldact, &temp_action, sizeof(struct sigaction_t)) != 0) {
        debugk("rt_sigaction: failed to copy default/ignore action to user\n");
        return -1;
      }
    }
  }

  // Set new action if provided
  if (user_act != NULL) {
    struct sigaction_t new_action;

    // Copy new action from user space
    if (copy_from_user(&new_action, user_act, sizeof(struct sigaction_t)) != 0) {
      debugk("rt_sigaction: failed to copy new action from user\n");
      return -1;
    }

    debugk("rt_sigaction: new handler=%p, flags=%d\n",
           new_action.sa_handler, new_action.sa_flags);

    // Free old action if it was allocated
    if (old_action != NULL &&
        old_action != (struct sigaction_t *)SIG_DEFAULT_HANDLER &&
        old_action != (struct sigaction_t *)SIG_IGNORE) {
      sigaction_t_free(old_action);
    }

    // Set new action
    if (new_action.sa_handler == (void (*)(int))SIG_DEFAULT_HANDLER ||
        new_action.sa_handler == (void (*)(int))SIG_IGNORE) {
      // Use special values directly
      current_task->signal_state.actions[signum] = (struct sigaction_t *)new_action.sa_handler;
    } else {
      // Allocate and copy
      struct sigaction_t *kernel_action = sigaction_t_alloc();
      if (kernel_action == NULL) {
        debugk("rt_sigaction: failed to allocate sigaction\n");
        return -1;
      }

      kernel_action->sa_handler = new_action.sa_handler;
      kernel_action->sa_mask = new_action.sa_mask;
      kernel_action->sa_flags = new_action.sa_flags;
      kernel_action->sa_restorer = new_action.sa_restorer;

      current_task->signal_state.actions[signum] = kernel_action;
    }

    debugk("rt_sigaction: successfully set handler for signal %d\n", signum);
  }

  return 0;
}
