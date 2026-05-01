#ifndef SCHEDULE_H
#define SCHEDULE_H

void schedule();

// Function that new tasks jump to after their first switch_to
// This is set up as a fake return address on new task's kernel stack
void fresh_task_jump(void);

#endif
