#include <setjmp.h>
#include <signal.h>

__attribute__((noinline))
int sigsetjmp(sigjmp_buf env, int savemask)
{
  env->__savemask = (unsigned long)savemask;
  if (savemask) {
    if (sigprocmask(SIG_SETMASK, (const sigset_t *)0, &env->__saved_mask) < 0)
      return -1;
  }
  return setjmp(env);
}
