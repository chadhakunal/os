#ifndef SCHEDULE_H
#define SCHEDULE_H

void schedule();

// Entry point for newly created tasks (defined in task_entry.S)
extern void task_entry(void);

#endif
