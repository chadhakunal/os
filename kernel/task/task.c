#include "kernel/task/task.h"

struct task_struct *create_init_process() {
  struct task_struct_t *init_task = task_t_alloc();
  return init_task;
}
