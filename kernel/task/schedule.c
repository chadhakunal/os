#include "kernel/task/schedule.h"
#include "kernel/task/task.h"
#include "lib/list.h"
#include "lib/printk/printk.h"

struct task_t *pick_next_task() {
  list_for_each(&task_list, pos) {
    struct task_t *task = container_of(pos, struct task_t, task_list);
    if (task->state == TASK_READY && task != current_task) {
      return task;
    }
  }

  return current_task;
}

void schedule() {
  struct task_t *next_task = pick_next_task();

  if (next_task == current_task) {
    return;
  }

  struct task_t *prev = current_task;

  if (prev->state == TASK_RUNNING) {
    prev->state = TASK_READY;
  }
  next_task->state = TASK_RUNNING;

  set_current_task(next_task);
  switch_to_page_table(next_task);
  switch_to(prev, next_task);
}

