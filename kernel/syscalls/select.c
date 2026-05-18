#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/trap.h"
#include "kernel/filesystem/poll.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "lib/list.h"
#include "errno.h"
#include "types.h"

#define FD_SETSIZE 1024
#define ULONG_BITS (8 * sizeof(unsigned long))
#define FD_SET_LONGS (FD_SETSIZE / ULONG_BITS)

struct kern_fd_set {
  unsigned long fds_bits[FD_SET_LONGS];
};

struct kern_timeval {
  long tv_sec;
  long tv_usec;
};

struct kern_timespec {
  long tv_sec;
  long tv_nsec;
};

static int fdset_isset(struct kern_fd_set *s, int fd) {
  return !!(s->fds_bits[fd / ULONG_BITS] & (1UL << (fd % ULONG_BITS)));
}


/*
 * Core: convert fd_sets to pollfd array, call do_poll, write results back.
 * do_poll is defined in poll.c — declare it here.
 */
extern int64_t do_poll(struct pollfd *kpfds, nfds_t nfds, int timeout_ms,
                       sigset_t *new_mask);

static int64_t do_select(int nfds,
                         struct kern_fd_set *kr, struct kern_fd_set *kw,
                         struct kern_fd_set *ke, int timeout_ms,
                         sigset_t *new_mask) {
  if (nfds < 0 || nfds > FD_SETSIZE)
    return -EINVAL;

  /* Build pollfd array from the three fd_sets. */
  struct pollfd pfds[FD_SETSIZE];
  nfds_t npfds = 0;

  for (int fd = 0; fd < nfds; fd++) {
    int r = kr && fdset_isset(kr, fd);
    int w = kw && fdset_isset(kw, fd);
    int e = ke && fdset_isset(ke, fd);
    if (!r && !w && !e) continue;
    pfds[npfds].fd     = fd;
    pfds[npfds].events = 0;
    if (r) pfds[npfds].events |= POLLIN;
    if (w) pfds[npfds].events |= POLLOUT;
    if (e) pfds[npfds].events |= POLLERR;
    pfds[npfds].revents = 0;
    npfds++;
  }

  int64_t ret = do_poll(pfds, npfds, timeout_ms, new_mask);
  if (ret <= 0)
    return ret;

  /* Zero out the sets and re-fill with ready fds. */
  if (kr) for (int i = 0; i < (int)FD_SET_LONGS; i++) kr->fds_bits[i] = 0;
  if (kw) for (int i = 0; i < (int)FD_SET_LONGS; i++) kw->fds_bits[i] = 0;
  if (ke) for (int i = 0; i < (int)FD_SET_LONGS; i++) ke->fds_bits[i] = 0;

  int64_t ready = 0;
  for (nfds_t i = 0; i < npfds; i++) {
    if (!pfds[i].revents) continue;
    int fd = pfds[i].fd;
    if (kr && (pfds[i].revents & (POLLIN  | POLLHUP))) {
      kr->fds_bits[fd / ULONG_BITS] |= (1UL << (fd % ULONG_BITS));
      ready++;
    }
    if (kw && (pfds[i].revents & POLLOUT)) {
      kw->fds_bits[fd / ULONG_BITS] |= (1UL << (fd % ULONG_BITS));
      ready++;
    }
    if (ke && (pfds[i].revents & (POLLERR | POLLNVAL))) {
      ke->fds_bits[fd / ULONG_BITS] |= (1UL << (fd % ULONG_BITS));
      ready++;
    }
  }
  return ready;
}

/* sys_select(int nfds, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv) */
DEFINE_SYSCALL5(select, int, nfds, void *, user_r, void *, user_w,
                void *, user_e, void *, user_tv) {
  struct kern_fd_set kr, kw, ke;
  struct kern_fd_set *pkr = NULL, *pkw = NULL, *pke = NULL;

  if (user_r) {
    if (copy_from_user(&kr, user_r, sizeof(kr)) != 0) return -EFAULT;
    pkr = &kr;
  }
  if (user_w) {
    if (copy_from_user(&kw, user_w, sizeof(kw)) != 0) return -EFAULT;
    pkw = &kw;
  }
  if (user_e) {
    if (copy_from_user(&ke, user_e, sizeof(ke)) != 0) return -EFAULT;
    pke = &ke;
  }

  int timeout_ms = -1;
  if (user_tv) {
    struct kern_timeval tv;
    if (copy_from_user(&tv, user_tv, sizeof(tv)) != 0) return -EFAULT;
    timeout_ms = (int)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
  }

  int64_t ret = do_select(nfds, pkr, pkw, pke, timeout_ms, NULL);

  if (ret > 0) {
    if (pkr && copy_to_user(user_r, pkr, sizeof(kr)) != 0) return -EFAULT;
    if (pkw && copy_to_user(user_w, pkw, sizeof(kw)) != 0) return -EFAULT;
    if (pke && copy_to_user(user_e, pke, sizeof(ke)) != 0) return -EFAULT;
  }
  return ret;
}

/* Linux pselect6 passes sigmask as a {sigset_t *, size_t} pair in arg6. */
struct pselect6_sigmask {
  sigset_t *ptr;
  uint64_t  size;
};

/* sys_pselect6(int nfds, fd_set *r, fd_set *w, fd_set *e,
 *              const struct timespec *ts,
 *              const struct pselect6_sigmask *sigmask_pair) */
DEFINE_SYSCALL6(pselect6, int, nfds, void *, user_r, void *, user_w,
                void *, user_e, void *, user_ts, void *, user_sigmask_pair) {
  struct kern_fd_set kr, kw, ke;
  struct kern_fd_set *pkr = NULL, *pkw = NULL, *pke = NULL;

  if (user_r) {
    if (copy_from_user(&kr, user_r, sizeof(kr)) != 0) return -EFAULT;
    pkr = &kr;
  }
  if (user_w) {
    if (copy_from_user(&kw, user_w, sizeof(kw)) != 0) return -EFAULT;
    pkw = &kw;
  }
  if (user_e) {
    if (copy_from_user(&ke, user_e, sizeof(ke)) != 0) return -EFAULT;
    pke = &ke;
  }

  int timeout_ms = -1;
  if (user_ts) {
    struct kern_timespec ts;
    if (copy_from_user(&ts, user_ts, sizeof(ts)) != 0) return -EFAULT;
    timeout_ms = (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
  }

  sigset_t saved_mask = current_task->signal_state.blocked;
  current_task->sigsuspend_saved_mask = saved_mask;

  int has_sigmask = 0;
  sigset_t new_mask = saved_mask;
  if (user_sigmask_pair) {
    struct pselect6_sigmask pair;
    if (copy_from_user(&pair, user_sigmask_pair, sizeof(pair)) != 0)
      return -EFAULT;
    if (pair.ptr) {
      if (copy_from_user(&new_mask, pair.ptr, sizeof(new_mask)) != 0)
        return -EFAULT;
      has_sigmask = 1;
    }
  }

  int64_t ret = do_select(nfds, pkr, pkw, pke, timeout_ms,
                          has_sigmask ? &new_mask : NULL);

  if (ret == -EINTR && has_sigmask)
    current_task->sigsuspend_active = 1;
  else
    current_task->signal_state.blocked = saved_mask;

  if (ret > 0) {
    if (pkr && copy_to_user(user_r, pkr, sizeof(kr)) != 0) return -EFAULT;
    if (pkw && copy_to_user(user_w, pkw, sizeof(kw)) != 0) return -EFAULT;
    if (pke && copy_to_user(user_e, pke, sizeof(ke)) != 0) return -EFAULT;
  }
  return ret;
}
