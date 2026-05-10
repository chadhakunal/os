#ifndef SIGNAL_H
#define SIGNAL_H

#include "types.h"
#include "lib/list.h"

// Signal numbers (standard POSIX)
#define SIGHUP    1   // Hangup
#define SIGINT    2   // Interrupt (Ctrl-C)
#define SIGQUIT   3   // Quit
#define SIGILL    4   // Illegal instruction
#define SIGTRAP   5   // Trace trap
#define SIGABRT   6   // Abort
#define SIGBUS    7   // Bus error
#define SIGFPE    8   // Floating point exception
#define SIGKILL   9   // Kill (uncatchable)
#define SIGUSR1   10  // User-defined signal 1
#define SIGSEGV   11  // Segmentation fault
#define SIGUSR2   12  // User-defined signal 2
#define SIGPIPE   13  // Broken pipe
#define SIGALRM   14  // Alarm clock
#define SIGTERM   15  // Termination
#define SIGSTKFLT 16  // Stack fault
#define SIGCHLD   17  // Child status changed
#define SIGCONT   18  // Continue
#define SIGSTOP   19  // Stop (uncatchable)
#define SIGTSTP   20  // Terminal stop
#define SIGTTIN   21  // Background read from tty
#define SIGTTOU   22  // Background write to tty
#define SIGURG    23  // Urgent condition on socket
#define SIGXCPU   24  // CPU time limit exceeded
#define SIGXFSZ   25  // File size limit exceeded
#define SIGVTALRM 26  // Virtual alarm clock
#define SIGPROF   27  // Profiling alarm clock
#define SIGWINCH  28  // Window size change
#define SIGIO     29  // I/O now possible
#define SIGPWR    30  // Power failure
#define SIGSYS    31  // Bad system call

#define NUM_SIGS 32  // Total number of signals

// Exit status when killed by signal (standard UNIX convention)
#define SIGNAL_EXIT_STATUS(sig) (128 + (sig))

// Signal set type (64 bits to hold signal mask)
typedef uint64_t sigset_t;

// Signal handler types
#define SIG_DEFAULT_HANDLER ((void (*)(int))0)
#define SIG_IGNORE ((void (*)(int))1)

// Signal action flags
#define SA_NOCLDSTOP  1    // Don't send SIGCHLD when children stop
#define SA_NOCLDWAIT  2    // Don't create zombies on child death
#define SA_SIGINFO    4    // Use sa_sigaction instead of sa_handler
#define SA_RESTART    8    // Restart syscalls interrupted by signal
#define SA_NODEFER    16   // Don't block signal while handler runs
#define SA_RESETHAND  32   // Reset to SIG_DFL after handler runs
#define SA_ONSTACK    64   // Execute handler on alternate signal stack

struct sigaction_t {
  void (*sa_handler)(int);              // Signal handler function
  sigset_t sa_mask;                     // Signals blocked during handler
  int sa_flags;                         // Flags (SA_*)
  void (*sa_restorer)(void);            // Signal jump point(user-space)
};

DEFINE_POOL(sigaction_t, struct sigaction_t)

struct signal_state_t {
  struct sigaction_t *actions[NUM_SIGS];
  sigset_t pending;
  sigset_t blocked;
};

#define clear_signal_set(set)      (*(set) = 0)
#define add_signal_to_set(set, sig)   (*(set) |= (1ULL << ((sig) - 1)))
#define delete_signal_from_set(set, sig)   (*(set) &= ~(1ULL << ((sig) - 1)))
#define sig_in_set(set, sig) ((*(set) & (1ULL << ((sig) - 1))) != 0)

struct trap_frame;

void check_and_deliver_signals(struct trap_frame *tf);
void send_signal_to_pgid(uint64_t pgid, int sig);

#endif
