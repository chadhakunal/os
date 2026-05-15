#ifndef TTY_H
#define TTY_H

#include "kernel/filesystem/vfs/vfs.h"
#include "lib/pool_allocator.h"
#include "types.h"

#define TIOCGPGRP 0x540F
#define TIOCSPGRP 0x5410
#define TCSRAW    0x5411  /* switch TTY to raw mode (no echo, no buffering) */
#define TCSCANON  0x5412  /* switch TTY to canonical mode (line-buffered, echo) */

struct tty_driver_t {
  struct file_ops_t file_ops;
  char tty_line_buffer[1024];
  uint64_t tty_line_buffer_size;
  bool buffer_ready;
  bool raw_mode;
  struct list_node wait_queue;
  uint64_t foreground_pgid;
};

DEFINE_POOL(tty_driver_t, struct tty_driver_t)

int64_t tty_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size);
int64_t tty_write(struct file_t *file, uint64_t offset, void *buffer, uint64_t size);
int tty_ioctl(struct file_t *file, unsigned long request, void *arg);
void tty_init(void);
void tty_receive(char *buffer, uint64_t size);
void tty_reset_buffer(void);

extern struct tty_driver_t tty_driver;

#endif
