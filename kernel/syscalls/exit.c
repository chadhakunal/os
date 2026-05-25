#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/memory/memory_info.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/task/signal.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"
#include "types.h"
#include "kernel/memory/page_allocator.h"

DEFINE_SYSCALL1(exit, int, status)
{
  /* If an unblockable terminating signal (e.g. SIGKILL) is pending, honour
   * it: the task was woken from a blocking syscall by the signal, returned
   * -EINTR to userspace, but the userspace code called exit() before
   * check_and_deliver_signals could run (e.g. sleep()/nanosleep() caller
   * ignores EINTR and exits cleanly).  Use the signal's wait-status encoding
   * so the parent sees WIFSIGNALED=true instead of WIFEXITED=true. */
  sigset_t pending = current_task->signal_state.pending
                     & ~current_task->signal_state.blocked;
  for (int sig = 1; sig < NUM_SIGS; sig++) {
    if (!(pending & (1ULL << (sig - 1))))
      continue;
    struct sigaction_t *act = current_task->signal_state.actions[sig];
    bool is_ign = (act == (struct sigaction_t *)SIG_IGNORE);
    bool is_dfl_term =
        (act == NULL || act == (struct sigaction_t *)SIG_DEFAULT_HANDLER) &&
        (sig == SIGKILL || sig == SIGTERM || sig == SIGHUP  || sig == SIGINT ||
         sig == SIGUSR1 || sig == SIGUSR2 || sig == SIGPIPE || sig == SIGALRM ||
         sig == SIGSEGV || sig == SIGBUS  || sig == SIGILL  || sig == SIGFPE  ||
         sig == SIGABRT || sig == SIGQUIT || sig == SIGTRAP || sig == SIGXFSZ);
    if (!is_ign && is_dfl_term) {
      debugk("exit: PID %llu has pending signal %d, overriding exit status\n",
             current_task->pid, sig);
      task_cleanup(SIGNAL_EXIT_STATUS(sig));
      schedule();
      panic("exit: returned from schedule()!");
    }
  }

  /* Linux wait(2) status: exit code in bits 8..15, low byte 0 for normal exit. */
  int wait_status = (status & 0xff) << 8;

  debugk("exit: PID %llu exiting with status %d (wait %d)\n",
         current_task->pid, status, wait_status);

  task_cleanup(wait_status);
  schedule();

  panic("exit: returned from schedule()!");
  return 0;
}
