#include "kernel/task.h"

struct task_struct *create_init_process() {
  struct task_struct *init_task = task_alloc();
}
