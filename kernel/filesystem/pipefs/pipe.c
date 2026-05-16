#include "kernel/filesystem/pipefs/pipe.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "kernel/user_data_access.h"
#include "errno.h"

struct pipe_t *pipe_create(void) {
  struct pipe_t *p = pipe_t_alloc();
  p->read_pos     = 0;
  p->write_pos    = 0;
  p->len          = 0;
  p->reader_count = 1;
  p->writer_count = 1;
  list_init(&p->wait_queue);
  return p;
}

int64_t pipe_read(struct pipe_t *pipe, void *user_buf, uint64_t size) {
  uint8_t *dst = (uint8_t *)user_buf;
  uint64_t copied = 0;

  while (copied < size) {
    while (pipe->len == 0) {
      if (pipe->writer_count == 0)
        return (int64_t)copied; /* EOF */

      list_append(&pipe->wait_queue, &current_task->wait_list);
      current_task->wait_reason = WAIT_IO;
      current_task->state       = TASK_BLOCKED;
      schedule();
    }

    uint8_t byte = pipe->buf[pipe->read_pos];
    pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUF_SIZE;
    pipe->len--;

    copy_to_user(&dst[copied], &byte, 1);
    copied++;
  }

  /* Wake any writer blocked on a full buffer. */
  wake_up(&pipe->wait_queue);

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
    }

    pipe->buf[pipe->write_pos] = src[written];
    pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUF_SIZE;
    pipe->len++;
    written++;
  }

  /* Wake any reader blocked waiting for data. */
  wake_up(&pipe->wait_queue);

  return (int64_t)written;
}

void pipe_close(struct pipe_t *pipe, int is_write_end) {
  if (is_write_end) {
    if (pipe->writer_count > 0)
      pipe->writer_count--;
    if (pipe->writer_count == 0)
      wake_up(&pipe->wait_queue); /* wake readers so they see EOF */
  } else {
    if (pipe->reader_count > 0)
      pipe->reader_count--;
    if (pipe->reader_count == 0)
      wake_up(&pipe->wait_queue); /* wake writers so they see EPIPE */
  }

  if (pipe->reader_count == 0 && pipe->writer_count == 0)
    pipe_t_free(pipe);
}
