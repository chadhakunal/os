#include "kernel/task/schedule.h"
#include "kernel/task/task.h"
#include "lib/list.h"

#define DEBUG 0
#include "lib/printk/printk.h"

struct scheduler_t scheduler;

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

  debugk("Scheduler initialized, added task PID %llu to active list\n", current_task->pid);
}

void move_to_expired(struct task_t *task) {
  debugk("Moving task PID %llu to expired list (runtime=%llu/%llu)\n",
         task->pid, task->runtime, task->max_runtime);
  list_remove(&task->scheduler_list);
  list_append(scheduler.expired_list, &task->scheduler_list);
  task->runtime = 0;
}

void swap_expired_active() {
  struct list_node *temp_sentinel = scheduler.active_list;
  scheduler.active_list = scheduler.expired_list;
  scheduler.expired_list = temp_sentinel;
}

void unblock_task(struct task_t *task) {
  if (task->state != TASK_BLOCKED) {
    return;
  }

  debugk("Unblocking task PID %llu\n", task->pid);

  task->state = TASK_READY;
  task->wait_reason = WAIT_NONE;
  task->wait_pid = 0;
  task->runtime = 0;

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
    // All tasks are blocked!
    // Lets run the idle task
    printk("switching to idle task!\n");
    return idle_task;
  }

  if (list_is_empty(scheduler.active_list)) {
    swap_expired_active();
  }

  // Select first task to run
  struct list_node *first_task_node = scheduler.active_list->next;
  struct task_t *next_task = container_of(first_task_node, struct task_t, scheduler_list);
  return next_task;
}

bool has_expired() {
  return current_task->runtime >= current_task->max_runtime;
}

void schedule() {
  // Handle special states that require immediate switch
  if (current_task->state == TASK_BLOCKED || current_task->state == TASK_ZOMBIE) {
    // Task is blocked or terminated, must switch away
    list_remove(&current_task->scheduler_list);
    if (current_task->state == TASK_BLOCKED) {
      list_append(scheduler.blocked_list, &current_task->scheduler_list);
    }
    // ZOMBIE tasks don't go on any list - they're waiting to be reaped
  } else if (has_expired()) {
    printk("About to move to expired\n");
    // Task used up its timeslice, move to expired
    move_to_expired(current_task);
  } else {
    // Task still has time left and is not blocked/zombie
    // Keep running it - no switch needed
    return;
  }

  struct task_t *next_task = pick_next_task();

  if (next_task == current_task) {
    // Could happen if there is only 1 task
    // it expires then active list is empty -> in pick_next_task we swap them and the only one available is the task that just ran
    return;
  }

  struct task_t *prev = current_task;

  if (prev->state == TASK_RUNNING) {
    prev->state = TASK_READY;
  }
  next_task->state = TASK_RUNNING;
  set_current_task(next_task);
  // Page table switch happens inside switch_to, after saving prev's context
  switch_to(prev, next_task);

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

