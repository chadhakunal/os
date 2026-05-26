#define DEBUG 0
#include "kernel/task/signal.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/task/elf_loader.h"
#include "kernel/signal_jump_point.h"
#include "arch/riscv64/trap.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/user_data_access.h"
#include "lib/printk/printk.h"
#include "errno.h"

struct signal_frame {
  struct trap_frame saved_tf;
  uint64_t signal_number;
  uint64_t old_blocked_mask;
};

static int get_pending_unblocked_signal(struct signal_state_t *sig_state) {
  sigset_t pending_unblocked = sig_state->pending & ~sig_state->blocked;

  if (pending_unblocked == 0) {
    return 0;
  }

  for (int sig = 1; sig < NUM_SIGS; sig++) {
    if (sig_in_set(&pending_unblocked, sig)) {
      return sig;
    }
  }

  return 0;
}

static bool handle_default_signal_action(int sig) {
  switch (sig) {
    case SIGKILL:
    case SIGHUP:
    case SIGINT:
    case SIGTERM:
    case SIGUSR1:
    case SIGUSR2:
    case SIGSEGV:
    case SIGXFSZ:
    case SIGABRT:
    case SIGQUIT:
    case SIGILL:
    case SIGFPE:
    case SIGBUS:
    case SIGPIPE:
    case SIGTRAP:
      debugk("signal: terminating process %llu due to signal %d\n", current_task->pid, sig);
      task_cleanup(SIGNAL_EXIT_STATUS(sig));
      debugk("signal: task_cleanup done, state=%d, calling schedule\n", current_task->state);
      schedule();
      debugk("signal: ERROR - schedule() returned for zombie PID %llu!\n", current_task->pid);
      return true;

    case SIGSTOP:
    case SIGTSTP:
    case SIGTTIN:
    case SIGTTOU:
      debugk("signal: stopping process %llu due to signal %d\n", current_task->pid, sig);
      current_task->stopped_sig = sig;
      current_task->state = TASK_STOPPED;
      /* Notify parent so waitpid(WUNTRACED) can return. */
      {
        struct task_t *parent = find_task_by_pid(current_task->ppid);
        if (parent) {
          send_signal(parent, SIGCHLD);
          if (parent->state == TASK_BLOCKED && parent->wait_reason == WAIT_CHILD)
            unblock_task(parent);
        }
      }
      /* Don't call schedule() here — trap frame isn't saved yet.
       * maybe_schedule_stopped() in trap_return handles the actual yield. */
      return true;

    case SIGCONT:
      /* Resume is handled in send_signal before the signal is ever queued.
       * If we somehow end up here the process is already running — discard. */
      return true;

    default:
      debugk("signal: ignoring signal %d (no handler implemented)\n", sig);
      return true;
  }
}

void send_signal(struct task_t *task, int sig) {
  if (task->state == TASK_ZOMBIE)
    return;

  if (sig == SIGCHLD)
    debugk("[SIGCHLD] send_signal: to PID %llu state=%d action=%p\n",
           task->pid, task->state, task->signal_state.actions[SIGCHLD]);

  if (sig == SIGCONT) {
    /* Discard any pending stop signals. */
    delete_signal_from_set(&task->signal_state.pending, SIGSTOP);
    delete_signal_from_set(&task->signal_state.pending, SIGTSTP);
    delete_signal_from_set(&task->signal_state.pending, SIGTTIN);
    delete_signal_from_set(&task->signal_state.pending, SIGTTOU);
    if (task->state == TASK_STOPPED) {
      task->stopped_sig = 0;
      task->stop_reported = 0;
      task->state = TASK_READY;
      task->wait_reason = WAIT_NONE;
      task->runtime = 0;
      list_remove(&task->scheduler_list);
      list_append(scheduler.expired_list, &task->scheduler_list);
    }
    /* Only queue SIGCONT for delivery if the task has a custom handler. */
    if (task->signal_state.actions[SIGCONT] &&
        task->signal_state.actions[SIGCONT] != (struct sigaction_t *)SIG_DEFAULT_HANDLER &&
        task->signal_state.actions[SIGCONT] != (struct sigaction_t *)SIG_IGNORE) {
      add_signal_to_set(&task->signal_state.pending, SIGCONT);
    }
    return;
  }

  /* Sending a stop signal to an already-stopped task is a no-op. */
  if (task->state == TASK_STOPPED &&
      (sig == SIGSTOP || sig == SIGTSTP || sig == SIGTTIN || sig == SIGTTOU))
    return;

  add_signal_to_set(&task->signal_state.pending, sig);

  if (sig == SIGCHLD)
    debugk("[SIGCHLD] queued, task state=%d wait_reason=%d blocked=%llx\n",
           task->state, task->wait_reason, task->signal_state.blocked);

  if (task->state == TASK_BLOCKED) {
    if (!sig_in_set(&task->signal_state.blocked, sig) ||
        task->wait_reason == WAIT_SIGNAL) {
      /* Don't wake a blocked task for a signal whose effective disposition
       * is SIG_IGN or SIG_DFL-discard (SIGCHLD, SIGCONT, SIGURG, SIGWINCH).
       * Those signals don't interrupt syscalls — only signals that will
       * actually be delivered to userspace or terminate the process do. */
      struct sigaction_t *act = task->signal_state.actions[sig];
      int is_ign = (act == (struct sigaction_t *)SIG_IGNORE);
      int is_dfl_discard = (act == NULL || act == (struct sigaction_t *)SIG_DEFAULT_HANDLER) &&
                           (sig == SIGCHLD || sig == SIGCONT ||
                            sig == SIGURG  || sig == SIGWINCH);
      if (!is_ign && !is_dfl_discard) {
        if (sig == SIGCHLD)
          debugk("[SIGCHLD] unblocking PID %llu\n", task->pid);
        unblock_task(task);
      } else if (task->wait_reason == WAIT_CHILD) {
        /* waitpid() must still wake for SIGCHLD even with default action. */
        unblock_task(task);
      }
    }
  } else if (task->state == TASK_STOPPED) {
    /* Any signal with a fatal default action must wake a stopped task so it
     * can be delivered. SIGKILL is unconditional; for others, only wake if
     * the disposition is not SIG_IGN and not a custom handler that could
     * legitimately be deferred — we wake for all non-ignored fatal signals. */
    struct sigaction_t *act = task->signal_state.actions[sig];
    int is_ign = (act == (struct sigaction_t *)SIG_IGNORE);
    int is_fatal_default = (act == NULL || act == (struct sigaction_t *)SIG_DEFAULT_HANDLER) &&
                           (sig == SIGKILL || sig == SIGTERM || sig == SIGHUP  ||
                            sig == SIGINT  || sig == SIGQUIT || sig == SIGPIPE ||
                            sig == SIGUSR1 || sig == SIGUSR2 || sig == SIGALRM ||
                            sig == SIGABRT || sig == SIGFPE  || sig == SIGILL  ||
                            sig == SIGSEGV || sig == SIGBUS  || sig == SIGTRAP ||
                            sig == SIGXCPU || sig == SIGXFSZ || sig == SIGSYS);
    int has_custom_handler = (act != NULL &&
                              act != (struct sigaction_t *)SIG_DEFAULT_HANDLER &&
                              act != (struct sigaction_t *)SIG_IGNORE);
    if (!is_ign && (is_fatal_default || has_custom_handler)) {
      task->stopped_sig = 0;
      task->stop_reported = 0;
      task->state = TASK_READY;
      task->wait_reason = WAIT_NONE;
      task->runtime = 0;
      list_remove(&task->scheduler_list);
      list_append(scheduler.expired_list, &task->scheduler_list);
    }
  }
}

void send_signal_to_pgid(uint64_t pgid, int sig) {
  list_for_each(&task_list, node) {
    struct task_t *task = container_of(node, struct task_t, task_list);
    if (task->pgid == pgid) {
      send_signal(task, sig);
    }
  }
}

void maybe_schedule_stopped(void) {
  while (current_task->state == TASK_STOPPED)
    schedule();
}

void check_and_deliver_signals(struct trap_frame *tf) {
  if (!current_task || (tf->sstatus & SSTATUS_SPP))
    return;

  int sig = get_pending_unblocked_signal(&current_task->signal_state);
  if (sig == 0)
    return;

  if (sig == SIGCHLD)
    debugk("[SIGCHLD] check_and_deliver: PID %llu action=%p\n",
           current_task->pid, current_task->signal_state.actions[sig]);

  struct sigaction_t *action = current_task->signal_state.actions[sig];

  if (action == (struct sigaction_t *)SIG_IGNORE) {
    delete_signal_from_set(&current_task->signal_state.pending, sig);
    return;
  }

  if (action == (struct sigaction_t *)SIG_DEFAULT_HANDLER || action == NULL) {
    delete_signal_from_set(&current_task->signal_state.pending, sig);
    handle_default_signal_action(sig);
    return;
  }

  if (sig == SIGCHLD)
    debugk("[SIGCHLD] dispatching custom handler=%p\n", action->sa_handler);

  /* SA_RESTART: if the signal interrupted a syscall that returned -EINTR,
   * rewind sepc to the ecall and restore the original a0 so the syscall
   * re-executes transparently after the handler returns via sigreturn. */
  if ((action->sa_flags & SA_RESTART) &&
      (int64_t)tf->a0 == -EINTR &&
      current_task->in_syscall == 0) {
    /* in_syscall was cleared after the syscall set a0=-EINTR, so the
     * saved restart_a0/a7 are valid from that syscall invocation. */
    tf->sepc -= 4;          /* rewind past the ecall */
    tf->a0    = current_task->restart_a0;
    tf->a7    = current_task->restart_a7;
    /* Don't dispatch the handler — just return to userspace to re-run ecall. */
    delete_signal_from_set(&current_task->signal_state.pending, sig);
    return;
  }

  uint64_t new_sp = (tf->sp - sizeof(struct signal_frame)) & ~15ULL;

  /* When returning from sigsuspend, restore the pre-sigsuspend mask via the frame */
  sigset_t frame_old_mask = current_task->sigsuspend_active
                            ? current_task->sigsuspend_saved_mask
                            : current_task->signal_state.blocked;
  current_task->sigsuspend_active = 0;

  copy_to_user((void *)new_sp, tf, sizeof(struct trap_frame));
  copy_to_user((void *)(new_sp + sizeof(struct trap_frame)), &sig, sizeof(uint64_t));
  copy_to_user((void *)(new_sp + sizeof(struct trap_frame) + 8), &frame_old_mask, sizeof(uint64_t));

  void (*handler_fn)(int) = action->sa_handler;
  sigset_t new_blocked = current_task->signal_state.blocked | action->sa_mask;
  if (!(action->sa_flags & SA_NODEFER)) {
    add_signal_to_set(&new_blocked, sig);
  }
  int resethand = action->sa_flags & SA_RESETHAND;
  current_task->signal_state.blocked = new_blocked;

  delete_signal_from_set(&current_task->signal_state.pending, sig);

  /* SA_RESETHAND: reset to SIG_DFL before running the handler */
  if (resethand) {
    sigaction_t_free(action);
    current_task->signal_state.actions[sig] = (struct sigaction_t *)SIG_DEFAULT_HANDLER;
  }

  current_task->signal_handler_depth++;

  tf->sp = new_sp;
  tf->sepc = (uint64_t)handler_fn;
  tf->ra = SIGNAL_JUMP_POINT_ADDR;
  tf->a0 = sig;
}
