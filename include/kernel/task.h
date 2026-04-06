#ifndef KERNEL_TASK
#define KERNEL_TASK

#include "trap"

struct task_struct {
  trap_frame* tf;
};

#endif
