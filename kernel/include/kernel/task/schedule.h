#ifndef SCHEDULE_H
#define SCHEDULE_H

struct scheduler_t {
  uint64_t quanta;
  struct list_node *active_list;
  struct list_node *expired_list;
  struct list_node *blocked_list;
};

extern scheduler_t scheduler;

void schedule();

// Function that new tasks jump to after their first switch_to
// This is set up as a fake return address on new task's kernel stack
void fresh_task_jump(void);

#endif
