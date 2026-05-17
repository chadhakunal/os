#define DEBUG 0
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/trap.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "lib/printk/printk.h"
#include "errno.h"
#include "types.h"

#define P_ALL  0
#define P_PID  1
#define P_PGID 2

/* Kernel-side siginfo_t (matches libc layout) */
struct kern_siginfo {
  int      si_signo;
  int      si_errno;
  int      si_code;
  uint64_t si_pid;
  int      si_uid;
  int      si_status;
  void    *si_addr;
  uint64_t si_value[2]; /* union sigval padding */
};

#define CLD_EXITED   1
#define CLD_KILLED   2

static struct task_t *find_zombie_waitid(struct task_t *parent, int idtype, uint64_t id) {
  list_for_each(&task_list, pos) {
    struct task_t *task = container_of(pos, struct task_t, task_list);
    if (task->ppid != parent->pid || task->state != TASK_ZOMBIE)
      continue;
    if (idtype == P_ALL)
      return task;
    if (idtype == P_PID && task->pid == id)
      return task;
    if (idtype == P_PGID && task->pgid == id)
      return task;
  }
  return NULL;
}

static bool has_alive_waitid(struct task_t *parent, int idtype, uint64_t id) {
  list_for_each(&task_list, pos) {
    struct task_t *task = container_of(pos, struct task_t, task_list);
    if (task->ppid != parent->pid || task->state == TASK_ZOMBIE)
      continue;
    if (idtype == P_ALL)
      return true;
    if (idtype == P_PID && task->pid == id)
      return true;
    if (idtype == P_PGID && task->pgid == id)
      return true;
  }
  return false;
}

DEFINE_SYSCALL4(waitid, int, idtype, uint64_t, id, struct kern_siginfo *, user_info, int, options)
{
  (void)options;

  while (1) {
    struct task_t *zombie = find_zombie_waitid(current_task, idtype, id);
    if (zombie) {
      struct kern_siginfo info = {0};
      info.si_signo  = 17; /* SIGCHLD */
      info.si_pid    = zombie->pid;
      info.si_uid    = 0;

      int exit_status = zombie->exit_status;
      int termsig = exit_status & 0x7f;
      if (termsig == 0) {
        info.si_code   = CLD_EXITED;
        info.si_status = (exit_status >> 8) & 0xff;
      } else {
        info.si_code   = CLD_KILLED;
        info.si_status = exit_status;
      }

      if (user_info && copy_to_user(user_info, &info, sizeof(info)) != 0)
        return -EFAULT;

      reap_zombie(zombie);
      return 0;
    }

    if (!has_alive_waitid(current_task, idtype, id))
      return -ECHILD;

    current_task->wait_reason = WAIT_CHILD;
    current_task->wait_pid    = (idtype == P_PID) ? (int64_t)id : -1;
    current_task->state       = TASK_BLOCKED;
    schedule();

    asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));

    sigset_t pending_unblocked = current_task->signal_state.pending
                                 & ~current_task->signal_state.blocked;
    if (pending_unblocked)
      return -EINTR;
  }
}
