#include "kernel/task/task.h"

struct task_t *create_init_process() {
  struct task_t *init_task = task_t_alloc();
  return init_task;
}
