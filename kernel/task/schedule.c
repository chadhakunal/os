#include "kernel/task/schedule.h"
#include "kernel/task/task.h"
#include "lib/list.h"
#include "lib/printk/printk.h"

struct scheduler_t scheduler;

void init_scheduler() {

}

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

