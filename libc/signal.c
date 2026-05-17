#include <arch/riscv64/syscall.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

int kill(pid_t pid, int sig) {
  return syscall2(SYS_kill, pid, sig);
}

int killpg(pid_t pgid, int sig) {
  return kill(-pgid, sig);
}

int raise(int sig) {
  return kill(getpid(), sig);
}

void (*signal(int signum, void (*handler)(int)))(int) {
  struct sigaction act, oldact;
  act.sa_handler = handler;
  act.sa_flags   = SA_RESTART;
  sigemptyset(&act.sa_mask);
  if (sigaction(signum, &act, &oldact) < 0)
    return SIG_ERR;
  return oldact.sa_handler;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  return syscall4(SYS_rt_sigaction, signum, act, oldact, sizeof(sigset_t));
}

int sigemptyset(sigset_t *set) {
  if (set == NULL) return -1;
  *set = 0;
  return 0;
}

int sigfillset(sigset_t *set) {
  if (set == NULL) return -1;
  *set = ~0UL;
  return 0;
}

int sigaddset(sigset_t *set, int signum) {
  if (set == NULL || signum < 1 || signum >= 32) return -1;
  *set |= (1UL << (signum - 1));
  return 0;
}

int sigdelset(sigset_t *set, int signum) {
  if (set == NULL || signum < 1 || signum >= 32) return -1;
  *set &= ~(1UL << (signum - 1));
  return 0;
}

int sigismember(const sigset_t *set, int signum) {
  if (set == NULL || signum < 1 || signum >= 32) return -1;
  return (*set & (1UL << (signum - 1))) != 0 ? 1 : 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  return syscall4(SYS_rt_sigprocmask, how, set, oldset, sizeof(sigset_t));
}

int sigpending(sigset_t *set) {
  return syscall2(SYS_rt_sigpending, set, sizeof(sigset_t));
}

int sigsuspend(const sigset_t *mask) {
  syscall2(SYS_rt_sigsuspend, mask, sizeof(sigset_t));
  errno = EINTR;
  return -1;
}

int sigwait(const sigset_t *set, int *sig) {
  (void)set; (void)sig;
  errno = ENOSYS;
  return -1;
}

int sigwaitinfo(const sigset_t *set, siginfo_t *info) {
  (void)set; (void)info;
  errno = ENOSYS;
  return -1;
}

int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout) {
  (void)set; (void)info; (void)timeout;
  errno = ENOSYS;
  return -1;
}

int sigqueue(pid_t pid, int sig, union sigval value) {
  (void)pid; (void)sig; (void)value;
  errno = ENOSYS;
  return -1;
}

int sigaltstack(const stack_t *ss, stack_t *old_ss) {
  (void)ss; (void)old_ss;
  errno = ENOSYS;
  return -1;
}

static const char *signame(int sig) {
  switch (sig) {
    case SIGHUP:  return "Hangup";
    case SIGINT:  return "Interrupt";
    case SIGQUIT: return "Quit";
    case SIGILL:  return "Illegal instruction";
    case SIGTRAP: return "Trace/breakpoint trap";
    case SIGABRT: return "Aborted";
    case SIGBUS:  return "Bus error";
    case SIGFPE:  return "Arithmetic exception";
    case SIGKILL: return "Killed";
    case SIGUSR1: return "User defined signal 1";
    case SIGSEGV: return "Segmentation fault";
    case SIGUSR2: return "User defined signal 2";
    case SIGPIPE: return "Broken pipe";
    case SIGALRM: return "Alarm clock";
    case SIGTERM: return "Terminated";
    case SIGCHLD: return "Child exited";
    case SIGCONT: return "Continued";
    case SIGSTOP: return "Stopped (signal)";
    case SIGTSTP: return "Stopped";
    default:      return "Unknown signal";
  }
}

void psignal(int sig, const char *msg) {
  if (msg && *msg)
    fprintf(stderr, "%s: %s\n", msg, signame(sig));
  else
    fprintf(stderr, "%s\n", signame(sig));
}

void psiginfo(const siginfo_t *info, const char *msg) {
  if (info)
    psignal(info->si_signo, msg);
}
