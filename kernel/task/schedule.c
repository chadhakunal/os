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
  printk("[schedule] Entered schedule()\n");
  printk("[schedule] current_task = %p, pid = %llu\n", current_task, current_task->pid);

  struct task_t *next_task = pick_next_task();
  printk("[schedule] pick_next_task returned %p, pid = %llu\n", next_task, next_task->pid);

  if (next_task == current_task) {
    printk("[schedule] No context switch needed, staying with current task\n");
    return;
  }

  printk("[schedule] Context switching from PID %llu to PID %llu\n",
         current_task->pid, next_task->pid);

  struct task_t *prev = current_task;

  if (prev->state == TASK_RUNNING) {
    prev->state = TASK_READY;
  }
  next_task->state = TASK_RUNNING;
  printk("[schedule] Updated task states\n");

  printk("[schedule] prev->kernel_context.sp = %p\n", (void*)prev->kernel_context.sp);
  printk("[schedule] next_task->kernel_context.sp = %p\n", (void*)next_task->kernel_context.sp);
  printk("[schedule] next_task->kernel_context.ra = %p\n", (void*)next_task->kernel_context.ra);

  printk("[schedule] Calling set_current_task()\n");
  set_current_task(next_task);

  printk("[schedule] Calling switch_to_page_table()\n");
  switch_to_page_table(next_task);

  printk("[schedule] Calling switch_to(prev=%p, next=%p)\n", prev, next_task);
  switch_to(prev, next_task);

  printk("[schedule] Returned from switch_to()\n");
}

