#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/trap.h"
#include "kernel/filesystem/poll.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/pipefs/pipe.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "lib/list.h"
#include "errno.h"
#include "types.h"

struct timespec {
  long tv_sec;
  long tv_nsec;
};

#define MAX_POLL_FDS 32

/*
 * Check all fds for readiness. Returns the number of fds with non-zero
 * revents, filling in pfds[i].revents. Returns -1 with a POLLNVAL fd on
 * bad fd.
 */
static int poll_scan(struct pollfd *pfds, nfds_t nfds) {
  int ready = 0;
  for (nfds_t i = 0; i < nfds; i++) {
    pfds[i].revents = 0;
    if (pfds[i].fd < 0) continue;
    struct file_t *file = find_file(&current_task->file_table, pfds[i].fd);
    if (!file) {
      pfds[i].revents = POLLNVAL;
      ready++;
      continue;
    }
    short rev = vfs_poll(file, pfds[i].events);
    if (rev) {
      pfds[i].revents = rev;
      ready++;
    }
  }
  return ready;
}

/*
 * Add this task to each pipe's wait_queue via the per-fd poll_waiter nodes.
 * Only pipe fds get a waiter; regular files are always ready so we never
 * block on them.
 */
static void poll_add_waiters(struct pollfd *pfds, nfds_t nfds,
                              struct poll_waiter *waiters) {
  for (nfds_t i = 0; i < nfds; i++) {
    waiters[i].task  = NULL;
    waiters[i].queue = NULL;
    list_init(&waiters[i].node);

    if (pfds[i].fd < 0) continue;
    struct file_t *file = find_file(&current_task->file_table, pfds[i].fd);
    if (!file || !file->pipe) continue;

    waiters[i].task  = current_task;
    waiters[i].queue = &file->pipe->poll_queue;
    list_append(&file->pipe->poll_queue, &waiters[i].node);
  }
}

/* Remove this task from every wait_queue it was added to. */
static void poll_remove_waiters(struct poll_waiter *waiters, nfds_t nfds) {
  for (nfds_t i = 0; i < nfds; i++) {
    if (waiters[i].queue && waiters[i].task) {
      list_remove(&waiters[i].node);
      waiters[i].queue = NULL;
      waiters[i].task  = NULL;
    }
  }
}

/* Core poll logic shared by sys_poll and sys_ppoll. */
static int64_t do_poll(struct pollfd *kpfds, nfds_t nfds, int timeout_ms,
                       sigset_t *saved_mask) {
  /* If there's a saved_mask (ppoll), swap it in now so signals that were
   * pending-but-blocked can fire while we're sleeping. */
  if (saved_mask)
    current_task->signal_state.blocked = *saved_mask;

  struct poll_waiter waiters[MAX_POLL_FDS];

  while (1) {
    int ready = poll_scan(kpfds, nfds);

    if (ready > 0) {
      if (saved_mask)
        current_task->signal_state.blocked = current_task->sigsuspend_saved_mask;
      return ready;
    }

    /* timeout == 0: don't block, return immediately */
    if (timeout_ms == 0) {
      if (saved_mask)
        current_task->signal_state.blocked = current_task->sigsuspend_saved_mask;
      return 0;
    }

    /* Check for pending signals before sleeping */
    sigset_t pending = current_task->signal_state.pending
                       & ~current_task->signal_state.blocked;
    if (pending) {
      if (saved_mask)
        current_task->signal_state.blocked = current_task->sigsuspend_saved_mask;
      return -EINTR;
    }

    /* Add ourselves to every relevant pipe wait_queue */
    poll_add_waiters(kpfds, nfds, waiters);

    current_task->wait_reason = WAIT_IO;
    current_task->state       = TASK_BLOCKED;
    schedule();

    /* Re-enable SUM after returning from scheduler */
    asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));

    /* Remove ourselves from all wait queues before re-scanning */
    poll_remove_waiters(waiters, nfds);

    /* Check if a signal woke us */
    pending = current_task->signal_state.pending
              & ~current_task->signal_state.blocked;
    if (pending) {
      if (saved_mask)
        current_task->signal_state.blocked = current_task->sigsuspend_saved_mask;
      return -EINTR;
    }
  }
}

/* sys_poll(struct pollfd *fds, nfds_t nfds, int timeout) */
DEFINE_SYSCALL3(poll, struct pollfd *, user_fds, nfds_t, nfds, int, timeout_ms) {
  if (nfds > MAX_POLL_FDS)
    return -EINVAL;

  struct pollfd kpfds[MAX_POLL_FDS];
  if (copy_from_user(kpfds, user_fds, nfds * sizeof(struct pollfd)) != 0)
    return -EFAULT;

  int64_t ret = do_poll(kpfds, nfds, timeout_ms, NULL);

  if (ret > 0)
    copy_to_user(user_fds, kpfds, nfds * sizeof(struct pollfd));

  return ret;
}

/* sys_ppoll(struct pollfd *fds, nfds_t nfds,
 *           const struct timespec *timeout, const sigset_t *sigmask) */
DEFINE_SYSCALL4(ppoll, struct pollfd *, user_fds, nfds_t, nfds,
                void *, user_timeout_raw, void *, user_sigmask_raw) {
  if (nfds > MAX_POLL_FDS)
    return -EINVAL;

  const struct timespec *user_timeout = (const struct timespec *)user_timeout_raw;
  const sigset_t        *user_sigmask = (const sigset_t *)user_sigmask_raw;

  struct pollfd kpfds[MAX_POLL_FDS];
  if (copy_from_user(kpfds, user_fds, nfds * sizeof(struct pollfd)) != 0)
    return -EFAULT;

  int timeout_ms = -1;
  if (user_timeout) {
    struct timespec ts;
    if (copy_from_user(&ts, user_timeout, sizeof(ts)) != 0)
      return -EFAULT;
    timeout_ms = (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
  }

  sigset_t saved_mask = current_task->signal_state.blocked;
  current_task->sigsuspend_saved_mask = saved_mask;

  sigset_t new_mask = saved_mask;
  if (user_sigmask) {
    if (copy_from_user(&new_mask, user_sigmask, sizeof(new_mask)) != 0)
      return -EFAULT;
  }

  int64_t ret = do_poll(kpfds, nfds, timeout_ms, &new_mask);

  /* Restore original signal mask */
  current_task->signal_state.blocked = saved_mask;

  if (ret > 0)
    copy_to_user(user_fds, kpfds, nfds * sizeof(struct pollfd));

  return ret;
}
