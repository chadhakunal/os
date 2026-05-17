#ifndef POLL_H
#define POLL_H

#include <sys/types.h>
#include <time.h>
#include <signal.h>

typedef unsigned int nfds_t;

struct pollfd {
  int   fd;
  short events;
  short revents;
};

#define POLLIN   0x001
#define POLLPRI  0x002
#define POLLOUT  0x004
#define POLLERR  0x008
#define POLLHUP  0x010
#define POLLNVAL 0x020
#define POLLRDNORM 0x040
#define POLLWRNORM 0x100

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout,
          const sigset_t *sigmask);

#endif
