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

int64_t pipe_read(struct pipe_t *pipe, void *user_buf, uint64_t size) {
  uint8_t *dst = (uint8_t *)user_buf;
  uint64_t copied = 0;

  /* Block only if no data is available yet. */
  while (pipe->len == 0) {
    if (pipe->writer_count == 0)
      return 0; /* EOF */

    list_append(&pipe->wait_queue, &current_task->wait_list);
    current_task->wait_reason = WAIT_IO;
    current_task->state       = TASK_BLOCKED;
    schedule();
    asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));
    sigset_t _pending = current_task->signal_state.pending
                        & ~current_task->signal_state.blocked;
    if (_pending)
      return -EINTR;
  }

  /* Copy whatever is available, up to size. */
  while (copied < size && pipe->len > 0) {
    uint8_t byte = pipe->buf[pipe->read_pos];
    pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUF_SIZE;
    pipe->len--;

    copy_to_user(&dst[copied], &byte, 1);
    copied++;
  }

  /* Wake any writer blocked on a full buffer, and any poll waiters. */
  wake_up(&pipe->wait_queue);
  wake_up_poll(&pipe->poll_queue);

  return (int64_t)copied;
}

int64_t pipe_write(struct pipe_t *pipe, const void *kernel_buf, uint64_t size) {
  if (pipe->reader_count == 0)
    return -EPIPE;

  const uint8_t *src = (const uint8_t *)kernel_buf;
  uint64_t written = 0;

  while (written < size) {
    while (pipe->len == PIPE_BUF_SIZE) {
      if (pipe->reader_count == 0)
        return written > 0 ? (int64_t)written : -EPIPE;

      list_append(&pipe->wait_queue, &current_task->wait_list);
      current_task->wait_reason = WAIT_IO;
      current_task->state       = TASK_BLOCKED;
      schedule();
      asm volatile("csrs sstatus, %0" :: "r"(SSTATUS_SUM));
      sigset_t _pending = current_task->signal_state.pending
                          & ~current_task->signal_state.blocked;
      if (_pending)
        return written > 0 ? (int64_t)written : -EINTR;
    }

    pipe->buf[pipe->write_pos] = src[written];
    pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUF_SIZE;
    pipe->len++;
    written++;
  }

  /* Wake any reader blocked waiting for data, and any poll waiters. */
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
