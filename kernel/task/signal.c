#define DEBUG 1
#include "kernel/task/signal.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/task/elf_loader.h"
#include "kernel/signal_jump_point.h"
#include "arch/riscv64/trap.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/user_data_access.h"
#include "lib/printk/printk.h"

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
    case SIGSEGV:
      debugk("signal: terminating process %llu due to signal %d\n", current_task->pid, sig);
      task_cleanup(SIGNAL_EXIT_STATUS(sig));
      debugk("signal: task_cleanup done, state=%d, calling schedule\n", current_task->state);
      schedule();
      debugk("signal: ERROR - schedule() returned for zombie PID %llu!\n", current_task->pid);
      return true;

    default:
      debugk("signal: ignoring signal %d (no handler implemented)\n", sig);
      return true;
  }
}

void send_signal(struct task_t *task, int sig) {
  if (task->state != TASK_ZOMBIE) {
    debugk("signal: sending signal %d to PID %llu\n", sig, task->pid);
    add_signal_to_set(&task->signal_state.pending, sig);
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

void check_and_deliver_signals(struct trap_frame *tf) {
  uint64_t tp_value;
  asm volatile("mv %0, tp" : "=r"(tp_value));
  debugk("check_and_deliver_signals: tf=%p, current_task=%p, tp=%p\n",
         tf, current_task, (void*)tp_value);

  if (!current_task || (tf->sstatus & SSTATUS_SPP)) {
    debugk("check_and_deliver_signals: returning early (current_task=%p, SPP=%d)\n",
           current_task, !!(tf->sstatus & SSTATUS_SPP));
    return;
  }

  int sig = get_pending_unblocked_signal(&current_task->signal_state);
  if (sig == 0) {
    return;
  }

  debugk("signal: delivering signal %d to PID %llu\n", sig, current_task->pid);

  struct sigaction_t *action = current_task->signal_state.actions[sig];

  if (action == (struct sigaction_t *)SIG_IGNORE) {
    debugk("signal: SIG_IGNORE for signal %d\n", sig);
    delete_signal_from_set(&current_task->signal_state.pending, sig);
    return;
  }

  if (action == (struct sigaction_t *)SIG_DEFAULT_HANDLER || action == NULL) {
    debugk("signal: SIG_DEFAULT_HANDLER for signal %d\n", sig);
    delete_signal_from_set(&current_task->signal_state.pending, sig);
    handle_default_signal_action(sig);
    return;
  }

  debugk("signal: custom handler at %p for signal %d\n", action->sa_handler, sig);

  uint64_t new_sp = (tf->sp - sizeof(struct signal_frame)) & ~15ULL;

  debugk("signal: current sp=%llx, new_sp=%llx, stack_start=0x%llx, stack_top=0x%llx\n",
         tf->sp, new_sp, DEFAULT_STACK_START, DEFAULT_STACK_TOP);

  if (new_sp < DEFAULT_STACK_START) {
    debugk("signal: ERROR - new_sp %llx is below stack start 0x%llx!\n", new_sp, DEFAULT_STACK_START);
  }

  copy_to_user((void *)new_sp, tf, sizeof(struct trap_frame));
  copy_to_user((void *)(new_sp + 288), &sig, sizeof(uint64_t));
  copy_to_user((void *)(new_sp + 296), &current_task->signal_state.blocked, sizeof(uint64_t));

  sigset_t new_blocked = current_task->signal_state.blocked | action->sa_mask;
  if (!(action->sa_flags & SA_NODEFER)) {
    add_signal_to_set(&new_blocked, sig);
  }
  current_task->signal_state.blocked = new_blocked;

  delete_signal_from_set(&current_task->signal_state.pending, sig);

  current_task->signal_handler_depth++;

  tf->sp = new_sp;
  tf->sepc = (uint64_t)action->sa_handler;
  tf->ra = SIGNAL_JUMP_POINT_ADDR;
  tf->a0 = sig;

  uint64_t tp_value_after;
  asm volatile("mv %0, tp" : "=r"(tp_value_after));
  debugk("signal: setup complete, handler=%llx, sp=%llx, ra=%llx\n",
         tf->sepc, tf->sp, tf->ra);
  debugk("signal: tf=%p, current_task=%p, tp=%p, &current_task->tf=%p\n",
         tf, current_task, (void*)tp_value_after, &current_task->tf);
}
