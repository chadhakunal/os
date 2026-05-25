#include "kernel/filesystem/pipefs/pipe.h"
#include "kernel/filesystem/poll.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "arch/riscv64/trap.h"
#include "errno.h"

struct pipe_t *pipe_create(void) {
  struct pipe_t *p = pipe_t_alloc();
  p->read_pos     = 0;
  p->write_pos    = 0;
  p->len          = 0;
  p->reader_count = 1;
  p->writer_count = 1;
  list_init(&p->wait_queue);
  list_init(&p->poll_queue);
  return p;
}

/* Returns true if any pending+unblocked signal will actually interrupt the
 * syscall — i.e., it has a custom handler or a terminating default action.
 * Signals whose default action is to be discarded (SIGCHLD, SIGCONT, SIGURG,
 * SIGWINCH) are excluded because send_signal() never wakes WAIT_IO tasks for
 * them; they accumulate in pending but must not cause -EINTR. */
static bool has_interrupting_signal(struct signal_state_t *s) {
  sigset_t pending = s->pending & ~s->blocked;
  for (int sig = 1; sig < NUM_SIGS; sig++) {
    if (!sig_in_set(&pending, sig))
      continue;
    struct sigaction_t *act = s->actions[sig];
    bool is_ign = (act == (struct sigaction_t *)SIG_IGNORE);
    bool is_dfl_discard =
        (act == NULL || act == (struct sigaction_t *)SIG_DEFAULT_HANDLER) &&
        (sig == SIGCHLD || sig == SIGCONT || sig == SIGURG || sig == SIGWINCH);
    if (!is_ign && !is_dfl_discard)
      return true;
  }
  return false;
}

int64_t pipe_read(struct pipe_t *pipe, void *user_buf, uint64_t size, bool nonblock) {
  uint8_t *dst = (uint8_t *)user_buf;
  uint64_t copied = 0;

  /* Block only if no data is available yet. */
  while (pipe->len == 0) {
    if (pipe->writer_count == 0)
      return 0; /* EOF */

    if (nonblock)
      return -EAGAIN;

    list_append(&pipe->wait_queue, &current_task->wait_list);
    current_task->wait_reason = WAIT_IO;
    current_task->state       = TASK_BLOCKED;
    schedule();
    asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));
    if (has_interrupting_signal(&current_task->signal_state))
      return -EINTR;
  }

  /* Copy whatever is available, up to size. */
  while (copied < size && pipe->len > 0) {
    uint8_t byte = pipe->buf[pipe->read_pos];
    pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUF_SIZE;
    pipe->len--;

    copy_to_user(&dst[copied], &byte, 1);
    copied++;

    /* Wake a blocked writer as soon as the first space opens. */
    if (pipe->len == PIPE_BUF_SIZE - 1) {
      wake_up(&pipe->wait_queue);
      wake_up_poll(&pipe->poll_queue);
    }
  }

  /* Final wake for any remaining waiters. */
  wake_up(&pipe->wait_queue);
  wake_up_poll(&pipe->poll_queue);

  return (int64_t)copied;
}

int64_t pipe_write(struct pipe_t *pipe, const void *user_buf, uint64_t size, bool nonblock) {
  if (pipe->reader_count == 0) {
    send_signal(current_task, SIGPIPE);
    return -EPIPE;
  }

  const uint8_t *src = (const uint8_t *)user_buf;
  uint64_t written = 0;

  while (written < size) {
    while (pipe->len == PIPE_BUF_SIZE) {
      if (pipe->reader_count == 0) {
        send_signal(current_task, SIGPIPE);
        return written > 0 ? (int64_t)written : -EPIPE;
      }

      if (nonblock)
        return written > 0 ? (int64_t)written : -EAGAIN;

      list_append(&pipe->wait_queue, &current_task->wait_list);
      current_task->wait_reason = WAIT_IO;
      current_task->state       = TASK_BLOCKED;
      schedule();
      asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));
      if (has_interrupting_signal(&current_task->signal_state))
        return written > 0 ? (int64_t)written : -EINTR;
    }

    uint8_t byte;
    copy_from_user(&byte, &src[written], 1);
    pipe->buf[pipe->write_pos] = byte;
    pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUF_SIZE;
    pipe->len++;
    written++;

    /* Wake a blocked reader as soon as the first byte lands. */
    if (pipe->len == 1) {
      wake_up(&pipe->wait_queue);
      wake_up_poll(&pipe->poll_queue);
    }
  }

  /* Final wake in case the reader is polling and missed the mid-write wakeup. */
  wake_up(&pipe->wait_queue);
  wake_up_poll(&pipe->poll_queue);

  return (int64_t)written;
}

short pipe_poll(struct pipe_t *pipe, int is_write_end, short events) {
  short revents = 0;
  if (!is_write_end) {
    if ((events & POLLIN) && pipe->len > 0)
      revents |= POLLIN;
    if (pipe->writer_count == 0)
      revents |= POLLHUP;
  } else {
    if ((events & POLLOUT) && pipe->len < PIPE_BUF_SIZE && pipe->reader_count > 0)
      revents |= POLLOUT;
    if (pipe->reader_count == 0)
      revents |= POLLERR;
  }
  return revents;
}

void pipe_close(struct pipe_t *pipe, int is_write_end) {
  if (is_write_end) {
    if (pipe->writer_count > 0)
      pipe->writer_count--;
    if (pipe->writer_count == 0) {
      wake_up(&pipe->wait_queue);
      wake_up_poll(&pipe->poll_queue);
    }
  } else {
    if (pipe->reader_count > 0)
      pipe->reader_count--;
    if (pipe->reader_count == 0) {
      wake_up(&pipe->wait_queue);
      wake_up_poll(&pipe->poll_queue);
    }
  }

  if (pipe->reader_count == 0 && pipe->writer_count == 0)
    pipe_t_free(pipe);
}
