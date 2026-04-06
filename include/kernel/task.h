#ifndef KERNEL_TASK
#define KERNEL_TASK

#include "trap"

struct task_struct {
  trap_frame tf;
  void *kernel_stack;
  struct page_table_t *root_page_table;
  uint64_t pid;
};

DEFINE_POOL(task, struct task_struct)

#endif
