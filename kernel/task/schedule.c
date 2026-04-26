#include "kernel/task/schedule.h"
#include "kernel/task/task.h"
#include "lib/list.h"

void schedule() {
  // Pick the next task to run
  struct task_t *next_task = pick_next_task();

  if (next_task == current_task) {
    // Nothing to do, just return to trap_handler
    return;
  }

  // We're switching tasks
  struct task_t *prev = current_task;

  // Update task states
  if (prev->state == TASK_RUNNING) {
    prev->state = TASK_READY;
  }
  next_task->state = TASK_RUNNING;

  // Update current_task pointer and tp register (BEFORE switch_to!)
  set_current_task(next_task);

  // Switch to next task's page table
  switch_to_page_table(next_task);

  // Switch kernel context (saves prev, restores next)
  // After this returns, we're running as the next task!
  switch_to(prev, next_task);

  // When we return here, we're now running as next_task
  // Just return - trap_handler will call trap_return(&current_task->tf)
}

struct task_t *pick_next_task() {
  // Simple round-robin: find next READY task in the list
  list_for_each(&task_list, pos) {
    struct task_t *task = container_of(pos, struct task_t, task_list);
    if (task->state == TASK_READY && task != current_task) {
      return task;
    }
  }

  // No other ready tasks, keep running current task
  return current_task;
}
