#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "types.h"
#include "lib/list.h"
#include "kernel/task/task.h"
#include "kernel/time/timer.h"

#define MAX_RUNTIME TIMER_INTERVAL_CYCLES*10

struct scheduler_t {
  struct list_node *active_list;
  struct list_node *expired_list;
  struct list_node *blocked_list;
};

extern struct scheduler_t scheduler;
extern int scheduler_ready;

void schedule();

void init_scheduler();

void unblock_task(struct task_t *task);

void wake_up(struct list_node *wait_queue);
void wake_up_poll(struct list_node *wait_queue);

void fresh_task_jump(void);

/*
 * Block the current task with the given wait_reason, sleep until something
 * calls unblock_task() on it, then return.
 *
 * Performs the pre-sleep signal check (handles the race where a signal arrived
 * while the task was still TASK_RUNNING) and the post-wake signal check.
 *
 * Returns true  if the task woke with no pending signal → caller may continue.
 * Returns false if a signal is pending → caller must return -EINTR immediately.
 *
 * Note: does NOT add/remove the task from any wait_queue — the caller is
 * responsible for that (some callers use wait_list, others use poll_waiter).
 */
bool task_block(enum wait_reason reason);

#endif
