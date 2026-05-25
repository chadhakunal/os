#define DEBUG 0
#include "kernel/task/schedule.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/filesystem/poll.h"
#include "kernel/panic.h"
#include "lib/list.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "arch/riscv64/trap.h"

#include "lib/printk/printk.h"

struct scheduler_t scheduler;
int scheduler_ready = 0;

void init_scheduler() {

  // Presumes init_task has been created and is the current_task

  scheduler.active_list = list_node_alloc();
  scheduler.expired_list = list_node_alloc(); // Set this to be empty
  scheduler.blocked_list = list_node_alloc();  // Set this to be empty

  // Initialize sentinels as circular lists
  list_init(scheduler.active_list);
  list_init(scheduler.expired_list);
  list_init(scheduler.blocked_list);

  // Add init task to active list
  list_append(scheduler.active_list, &current_task->scheduler_list);

  scheduler_ready = 1;
  debugk("Scheduler initialized, added task PID %llu to active list\n", current_task->pid);
}

void move_to_expired(struct task_t *task) {

  debugk("Moving task PID %llu to expired list (runtime=%llu/%llu)\n",
         task->pid, task->runtime, task->max_runtime);
  list_remove(&task->scheduler_list);
  debugk("Removed task from its scheduler list\n");
  list_append(scheduler.expired_list, &task->scheduler_list);
  task->runtime = 0;
}

void swap_expired_active() {
  struct list_node *temp_sentinel = scheduler.active_list;
  scheduler.active_list = scheduler.expired_list;
  scheduler.expired_list = temp_sentinel;
}

void unblock_task(struct task_t *task) {
  debugk("unblock_task: called for PID %llu (state=%d)\n", task->pid, task->state);

  if (task->state != TASK_BLOCKED) {
    debugk("unblock_task: PID %llu not blocked, ignoring\n", task->pid);
    return;
  }

  debugk("unblock_task: unblocking PID %llu\n", task->pid);

  task->state = TASK_READY;
  task->wait_reason = WAIT_NONE;
  task->wait_pid = 0;
  task->runtime = 0;

  debugk("unblock_task: PID %llu now in TASK_READY state\n", task->pid);

  list_remove(&task->scheduler_list);
  list_append(scheduler.expired_list, &task->scheduler_list);
}

void wake_up(struct list_node *wait_queue) {
  if (list_is_empty(wait_queue)) {
    return;
  }

  struct list_node *current = wait_queue->next;
  while (current != wait_queue) {
    struct task_t *task = container_of(current, struct task_t, wait_list);
    struct list_node *next = current->next;

    list_remove(&task->wait_list);
    unblock_task(task);

    current = next;
  }
}

/* Wake all poll_waiter entries on a queue without touching task->wait_list. */
void wake_up_poll(struct list_node *wait_queue) {
  if (list_is_empty(wait_queue))
    return;

  struct list_node *current = wait_queue->next;
  while (current != wait_queue) {
    struct poll_waiter *w = container_of(current, struct poll_waiter, node);
    struct list_node *next = current->next;
    list_remove(&w->node);
    w->queue = NULL;
    if (w->task && w->task->state == TASK_BLOCKED)
      unblock_task(w->task);
    current = next;
  }
}

struct task_t *pick_next_task() {
  if (list_is_empty(scheduler.active_list) && list_is_empty(scheduler.expired_list)) {
    return idle_task;
  }

  if (list_is_empty(scheduler.active_list)) {
    swap_expired_active();
  }

  // Select first task to run
  struct list_node *first_task_node = scheduler.active_list->next;
  struct task_t *next_task = container_of(first_task_node, struct task_t, scheduler_list);

  // Sanity check: verify the task looks valid
  if (next_task == NULL) {
    panic("pick_next_task: next_task is NULL!");
  }
  if (next_task->state != TASK_READY && next_task->state != TASK_RUNNING) {
    panic("pick_next_task: next_task has invalid state (state=%d, pid=%llu)!",
          next_task->state, next_task->pid);
  }
  if (next_task->mm_struct.root_satp == NULL) {
    panic("pick_next_task: next_task has NULL page table (pid=%llu)!", next_task->pid);
  }

  // Sanity check: verify kernel context looks valid
  // SP should be within or at the top of the kernel stack (stack grows down)
  if (next_task->kernel_context.sp < KERNEL_STACK_VIRTUAL_BASE ||
      next_task->kernel_context.sp > KERNEL_STACK_VIRTUAL_BASE + KERNEL_STACK_SIZE) {
    panic("pick_next_task: next_task has invalid kernel SP! pid=%llu sp=0x%llx (valid range: 0x%llx-0x%llx)",
          next_task->pid, next_task->kernel_context.sp,
          KERNEL_STACK_VIRTUAL_BASE, KERNEL_STACK_VIRTUAL_BASE + KERNEL_STACK_SIZE);
  }
  if (next_task->kernel_context.ra == 0) {
    panic("pick_next_task: next_task has NULL return address! pid=%llu", next_task->pid);
  }

  return next_task;
}

bool has_expired() {
  return current_task->runtime >= current_task->max_runtime;
}

void schedule() {
  extern struct task_t *idle_task;
  extern void trap_return(struct trap_frame *tf);

  if (!current_task)
    panic("schedule: current_task is NULL");

  if (current_task == idle_task) {
    struct task_t *next_task = pick_next_task();
    if (next_task == idle_task)
      return;
    next_task->state = TASK_RUNNING;
    set_current_task(next_task);
    switch_to(idle_task, next_task);
    trap_return(&current_task->tf);
  }

  if (current_task->state == TASK_BLOCKED ||
      current_task->state == TASK_STOPPED ||
      current_task->state == TASK_ZOMBIE) {
    list_remove(&current_task->scheduler_list);
    if (current_task->state == TASK_BLOCKED ||
        current_task->state == TASK_STOPPED) {
      list_append(scheduler.blocked_list, &current_task->scheduler_list);
    }
  } else if (has_expired()) {
    move_to_expired(current_task);
  } else {
    return;
  }

  struct task_t *next_task = pick_next_task();

  if (next_task == current_task) {
    current_task->runtime = 0;
    return;
  }

  struct task_t *prev = current_task;

  if (prev->state == TASK_RUNNING)
    prev->state = TASK_READY;
  next_task->state = TASK_RUNNING;

  set_current_task(next_task);
  switch_to(prev, next_task);

  /* Sanity check: after resuming, we must be RUNNING (not blocked/stopped). */
  if (current_task->state != TASK_RUNNING)
    panic("schedule: resumed task PID=%llu has unexpected state=%d",
          current_task->pid, current_task->state);
}

// Called when a newly created task is first scheduled
// This is the fake "return address" set up on new task's kernel stack
void fresh_task_jump(void) {
  // New task starts here after its first switch_to
  // Jump to user space
  extern void trap_return(struct trap_frame *tf);
  trap_return(&current_task->tf);
}

/*
 * Check whether a signal that would interrupt a blocking syscall is pending.
 * This covers signals with custom handlers and signals whose default action
 * terminates the process (SIGKILL, SIGTERM, etc.).  Signals whose default
 * action is to discard (SIGCHLD, SIGCONT, SIGURG, SIGWINCH) are excluded
 * because they must not cause -EINTR.
 */
static bool has_interrupting_signal_pending(void) {
  sigset_t pending = current_task->signal_state.pending
                     & ~current_task->signal_state.blocked;
  for (int sig = 1; sig < NUM_SIGS; sig++) {
    if (!(pending & (1ULL << (sig - 1))))
      continue;
    struct sigaction_t *act = current_task->signal_state.actions[sig];
    bool is_ign = (act == (struct sigaction_t *)SIG_IGNORE);
    bool is_dfl_discard =
        (act == NULL || act == (struct sigaction_t *)SIG_DEFAULT_HANDLER) &&
        (sig == SIGCHLD || sig == SIGCONT || sig == SIGURG || sig == SIGWINCH);
    if (!is_ign && !is_dfl_discard)
      return true;
  }
  return false;
}

bool task_block(enum wait_reason reason) {
  /* Pre-sleep check: catch signals that arrived while we were still TASK_RUNNING.
   * send_signal() can only call unblock_task() on a TASK_BLOCKED task; if a
   * signal arrived before we set state=TASK_BLOCKED, it would be stuck until
   * the next external wakeup (e.g. a 60-second sleep timer expiring). */
  if (has_interrupting_signal_pending())
    return false;

  current_task->wait_reason = reason;
  current_task->state       = TASK_BLOCKED;

  schedule();

  /* Re-enable supervisor access to user memory after returning from scheduler. */
  asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));

  /* Post-wake check: a signal (e.g. SIGKILL) woke us early. */
  return !has_interrupting_signal_pending();
}

