#pragma once

#include <sys/types.h>

#define WNOHANG    1
#define WUNTRACED  2

#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WIFEXITED(s)   (WTERMSIG(s) == 0)
#define WIFSIGNALED(s) (WTERMSIG(s) > 0 && WTERMSIG(s) <= 31)

pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);
