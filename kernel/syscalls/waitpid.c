#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "lib/printk/printk.h"
#include "types.h"
#include "errno.h"

#define WNOHANG   1
#define WUNTRACED 2

/* Encode stopped status: (stopsig << 8) | 0x7f — same as Linux/POSIX. */
#define W_STOPCODE(sig) (((sig) << 8) | 0x7f)

static struct task_t *find_stopped_child(struct task_t *parent,
                                          int64_t specific_pid, uint64_t pgid) {
  list_for_each(&task_list, pos) {
    struct task_t *task = container_of(pos, struct task_t, task_list);
    if (task->ppid != parent->pid || task->state != TASK_STOPPED)
      continue;
    if (specific_pid != -1 && task->pid != (uint64_t)specific_pid)
      continue;
    if (pgid != 0 && task->pgid != pgid)
      continue;
    if (task->stop_reported)
      continue;
    return task;
  }
  return NULL;
}

DEFINE_SYSCALL3(waitpid, int64_t, pid, int *, wstatus, int, options)
{
  if (options & ~(WNOHANG | WUNTRACED))
    return -EINVAL;

  int64_t  specific_pid = -1;
  uint64_t pgid         = 0;

  if (pid > 0)
    specific_pid = pid;
  else if (pid == 0)
    pgid = current_task->pgid;
  else if (pid < -1)
    pgid = (uint64_t)(-pid);

  while (1) {
    struct task_t *zombie = find_zombie_child(current_task, specific_pid, pgid);
    if (zombie) {
      if (wstatus)
        copy_to_user(wstatus, &zombie->exit_status, sizeof(int));
      uint64_t child_pid = zombie->pid;
      reap_zombie(zombie);
      return child_pid;
    }

    if (options & WUNTRACED) {
      struct task_t *stopped = find_stopped_child(current_task, specific_pid, pgid);
      if (stopped) {
        int status = W_STOPCODE(stopped->stopped_sig);
        if (wstatus)
          copy_to_user(wstatus, &status, sizeof(int));
        stopped->stop_reported = 1;
        return stopped->pid;
      }
    }

    if (!has_alive_children(current_task, specific_pid, pgid))
      return -ECHILD;

    if (options & WNOHANG)
      return 0;

    current_task->wait_reason = WAIT_CHILD;
    current_task->wait_pid    = specific_pid;
    current_task->state       = TASK_BLOCKED;
    schedule();

    asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));

    /* SIGCHLD wakes waitpid but doesn't interrupt it — clear it and loop. */
    current_task->signal_state.pending &= ~(1ULL << (SIGCHLD - 1));
    sigset_t pending_unblocked = current_task->signal_state.pending
                                 & ~current_task->signal_state.blocked;
    if (pending_unblocked)
      return -EINTR;
  }
}
