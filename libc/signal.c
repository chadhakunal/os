#include <signal.h>
#include <unistd.h>

int kill(pid_t pid, int sig) {
  return syscall2(SYS_kill, pid, sig);
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
