#pragma once

#include <sys/types.h>
#include <signal.h>

#define WNOHANG    1
#define WUNTRACED  2

#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WIFEXITED(s)   (WTERMSIG(s) == 0)
#define WIFSIGNALED(s) (WTERMSIG(s) > 0 && WTERMSIG(s) <= 31)
#define WIFSTOPPED(s)  (((s) & 0xff) == 0x7f)
#define WSTOPSIG(s)    (((s) >> 8) & 0xff)

typedef enum { P_ALL = 0, P_PID = 1, P_PGID = 2 } idtype_t;

pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);
int waitid(idtype_t idtype, pid_t id, siginfo_t *info, int options);
