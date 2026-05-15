#define DEBUG 0
#include "kernel/task/schedule.h"
#include "kernel/task/task.h"
#include "kernel/panic.h"
#include "lib/list.h"
#include "arch/riscv64/virtual_memory_init.h"

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

  debugk("[schedule] ENTERED - current_task PID=%llu, state=%d, runtime=%llu/%llu, PC=0x%llx\n",
         current_task->pid, current_task->state, current_task->runtime, current_task->max_runtime,
         current_task->tf.sepc);

  if (current_task == idle_task) {
    debugk("[schedule] current_task is idle, picking next task\n");
    struct task_t *next_task = pick_next_task();
    if (next_task == idle_task) {
      debugk("[schedule] only idle task available, returning\n");
      return;
    }
    debugk("[schedule] picked PID %llu from idle\n", next_task->pid);
    next_task->state = TASK_RUNNING;
    debugk("[schedule] ABOUT TO CALL set_current_task (from idle)\n");
    set_current_task(next_task);
    debugk("[schedule] ABOUT TO CALL switch_to from idle to PID %llu\n", next_task->pid);
    switch_to(idle_task, next_task);
    // After switch_to, we're now running as next_task
    // Jump to user mode
    debugk("[schedule] ABOUT TO CALL trap_return after switch from idle\n");
    trap_return(&current_task->tf);
  }

  if (current_task->state == TASK_BLOCKED || current_task->state == TASK_ZOMBIE) {
    debugk("[schedule] current_task PID=%llu is %s, moving to appropriate list\n",
           current_task->pid, current_task->state == TASK_BLOCKED ? "BLOCKED" : "ZOMBIE");
    list_remove(&current_task->scheduler_list);
    if (current_task->state == TASK_BLOCKED) {
      list_append(scheduler.blocked_list, &current_task->scheduler_list);
    }
  } else if (has_expired()) {
    debugk("[schedule] current_task PID=%llu has expired, moving to expired list\n", current_task->pid);
    move_to_expired(current_task);
  } else {
    debugk("[schedule] current_task PID=%llu not expired and not blocked, returning without switch\n", current_task->pid);
    return;
  }

  debugk("[schedule] picking next task\n");
  struct task_t *next_task = pick_next_task();
  debugk("[schedule] picked next_task PID=%llu\n", next_task->pid);

  if (next_task == current_task) {
    // Could happen if there is only 1 task
    // it expires then active list is empty -> in pick_next_task we swap them and the only one available is the task that just ran
    debugk("[schedule] next_task is same as current_task, resetting runtime and returning\n");
    current_task->runtime = 0;  // Reset runtime so it doesn't immediately expire again
    return;
  }

  struct task_t *prev = current_task;

  if (prev->state == TASK_RUNNING) {
    prev->state = TASK_READY;
  }
  next_task->state = TASK_RUNNING;

  debugk("[schedule] Switching from PID %llu (state=%d) to PID %llu (state=%d)\n",
         prev->pid, prev->state, next_task->pid, next_task->state);
  debugk("[schedule] next_task kernel_context: sp=0x%llx ra=0x%llx\n",
         next_task->kernel_context.sp, next_task->kernel_context.ra);
  debugk("[schedule] ABOUT TO CALL set_current_task\n");

  set_current_task(next_task);

  debugk("[schedule] ABOUT TO CALL switch_to(prev=%llu, next=%llu)\n", prev->pid, next_task->pid);
  switch_to(prev, next_task);

  debugk("[schedule] RETURNED after context switch - now running as PID %llu\n", current_task->pid);
  // When we return here, we've been rescheduled
  // Just return to caller (either trap_handler or kernel code)
}

// Called when a newly created task is first scheduled
// This is the fake "return address" set up on new task's kernel stack
void fresh_task_jump(void) {
  // New task starts here after its first switch_to
  // Jump to user space
  extern void trap_return(struct trap_frame *tf);
  trap_return(&current_task->tf);
}

