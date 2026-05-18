#pragma once

#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define FD_SETSIZE 1024

typedef struct {
  unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;

#define __FD_ELT(d)  ((d) / (8 * sizeof(unsigned long)))
#define __FD_MASK(d) (1UL << ((d) % (8 * sizeof(unsigned long))))

#define FD_ZERO(s)      do { \
  fd_set *_s = (s); \
  for (int _i = 0; _i < (int)(FD_SETSIZE / (8 * sizeof(unsigned long))); _i++) \
    _s->fds_bits[_i] = 0; \
} while (0)
#define FD_SET(d, s)    ((s)->fds_bits[__FD_ELT(d)] |=  __FD_MASK(d))
#define FD_CLR(d, s)    ((s)->fds_bits[__FD_ELT(d)] &= ~__FD_MASK(d))
#define FD_ISSET(d, s)  (!!((s)->fds_bits[__FD_ELT(d)] & __FD_MASK(d)))

struct timeval {
  long tv_sec;
  long tv_usec;
};

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout);
int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            const struct timespec *timeout, const sigset_t *sigmask);
