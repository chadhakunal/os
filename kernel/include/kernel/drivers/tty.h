#ifndef TTY_H
#define TTY_H

#include "kernel/filesystem/vfs/vfs.h"
#include "lib/pool_allocator.h"
#include "types.h"

struct tty_driver_t {
  struct file_ops_t file_ops;
  char tty_line_buffer[1024];
  uint64_t tty_line_buffer_size;
  bool buffer_ready;
  struct list_node wait_queue;
};

DEFINE_POOL(tty_driver_t, struct tty_driver_t)

int64_t tty_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size);
int64_t tty_write(struct file_t *file, uint64_t offset, void *buffer, uint64_t size);
void tty_init(void);
void tty_receive(char *buffer, uint64_t size);
void tty_reset_buffer(void);

extern struct tty_driver_t tty_driver;

#endif
