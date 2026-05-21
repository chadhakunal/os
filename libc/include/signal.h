#ifndef _SIGNAL_H
#define _SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <types.h>
#include <time.h>

// Signal numbers (POSIX standard)
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPWR    30
#define SIGSYS    31

// Signal handler constants
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
#define SIG_ERR ((void (*)(int))-1)

// Signal set type
typedef unsigned long sigset_t;

typedef void (*sighandler_t)(int);

// Atomic integer type for use in signal handlers
typedef int sig_atomic_t;

// Signal value union (used by siginfo and sigqueue)
union sigval {
  int   sival_int;
  void *sival_ptr;
};

// sigevent notification types
#define SIGEV_NONE   0
#define SIGEV_SIGNAL 1
#define SIGEV_THREAD 2

struct sigevent {
  int          sigev_notify;
  int          sigev_signo;
  union sigval sigev_value;
  void       (*sigev_notify_function)(union sigval);
  void        *sigev_notify_attributes;
};

// Signal info structure (SA_SIGINFO handlers, sigwaitinfo, etc.)
typedef struct {
  int          si_signo;
  int          si_errno;
  int          si_code;
  pid_t        si_pid;
  int          si_uid;
  int          si_status;
  void        *si_addr;
  union sigval si_value;
} siginfo_t;

// si_code values
#define SI_USER    0
#define SI_KERNEL  128
#define SI_QUEUE  -1
#define SI_TIMER  -2
#define SI_ASYNCIO -4

// Alternate signal stack
#define SIGSTKSZ  8192
#define MINSIGSTKSZ 2048
#define SS_ONSTACK  1
#define SS_DISABLE  2

typedef struct {
  void  *ss_sp;
  size_t ss_size;
  int    ss_flags;
} stack_t;

// Signal action flags
#define SA_NOCLDSTOP  1
#define SA_NOCLDWAIT  2
#define SA_SIGINFO    4
#define SA_RESTART    8
#define SA_NODEFER    16
#define SA_RESETHAND  32
#define SA_ONSTACK    64

// Signal action structure
struct sigaction {
  union {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, siginfo_t *, void *);
  };
  sigset_t sa_mask;
  int sa_flags;
  void (*sa_restorer)(void);
};

// Signal mask manipulation constants
#define SIG_BLOCK     0
#define SIG_UNBLOCK   1
#define SIG_SETMASK   2

// Function declarations
int kill(pid_t pid, int sig);
int killpg(pid_t pgid, int sig);
int raise(int sig);
void (*signal(int signum, void (*handler)(int)))(int);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigpending(sigset_t *set);
int sigsuspend(const sigset_t *mask);
int sigwait(const sigset_t *set, int *sig);
int sigwaitinfo(const sigset_t *set, siginfo_t *info);
int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout);
int sigqueue(pid_t pid, int sig, union sigval value);
int sigaltstack(const stack_t *ss, stack_t *old_ss);
void psignal(int sig, const char *msg);
void psiginfo(const siginfo_t *info, const char *msg);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigdelset(sigset_t *set, int signum);
int sigismember(const sigset_t *set, int signum);

#define SIG2STR_MAX 17
int sig2str(int signum, char *str);
int str2sig(const char *str, int *signum);

#ifdef __cplusplus
}
#endif

#endif // _SIGNAL_H
