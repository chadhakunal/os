#ifndef KERNEL_POLL_H
#define KERNEL_POLL_H

#include "types.h"
#include "lib/list.h"
#include "kernel/task/task.h"

#define POLLIN   0x001
#define POLLPRI  0x002
#define POLLOUT  0x004
#define POLLERR  0x008
#define POLLHUP  0x010
#define POLLNVAL 0x020

struct pollfd {
  int   fd;
  short events;
  short revents;
};

typedef unsigned int nfds_t;

/* One waiter entry — allocated on the poll syscall's stack, one per fd.
 * Added to the fd's wait_queue so that pipe state changes wake the poller. */
struct poll_waiter {
  struct task_t    *task;
  struct list_node  node;       /* sits on pipe->wait_queue */
  struct list_node *queue;      /* back-pointer to the queue we joined */
};

int64_t sys_poll(struct trap_frame *tf);
int64_t sys_ppoll(struct trap_frame *tf);

#endif
