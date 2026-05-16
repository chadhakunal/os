#ifndef PIPE_H
#define PIPE_H

#include "types.h"
#include "lib/pool_allocator.h"
#include "lib/list.h"

#define PIPE_BUF_SIZE 2048

struct pipe_t {
  uint8_t          buf[PIPE_BUF_SIZE];
  uint32_t         read_pos;
  uint32_t         write_pos;
  uint32_t         len;
  uint32_t         reader_count;
  uint32_t         writer_count;
  struct list_node wait_queue;
};

DEFINE_POOL(pipe_t, struct pipe_t)

struct pipe_t *pipe_create(void);

int64_t pipe_read (struct pipe_t *pipe, void *user_buf, uint64_t size);
int64_t pipe_write(struct pipe_t *pipe, const void *kernel_buf, uint64_t size);

/* is_write_end: 1 = write end closing, 0 = read end closing */
void pipe_close(struct pipe_t *pipe, int is_write_end);

#endif
