#include "kernel/drivers/tty.h"
#include "lib/printk/printk.h"
#include "kernel/task/schedule.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/user_data_access.h"
#include "lib/string.h"
#include "errno.h"

int64_t tty_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  if (!tty_driver.buffer_ready) {
    debugk("[TTY] PID %llu blocking on read, buffer_ready=%d\n",
           current_task->pid, tty_driver.buffer_ready);
    list_append(&tty_driver.wait_queue, &current_task->wait_list);
    current_task->state = TASK_BLOCKED;
    current_task->wait_reason = WAIT_IO;
    schedule();
    debugk("[TTY] PID %llu woke up from read, buffer_ready=%d\n",
           current_task->pid, tty_driver.buffer_ready);
  }

  uint64_t bytes_to_copy = tty_driver.tty_line_buffer_size < size
                           ? tty_driver.tty_line_buffer_size : size;

  memcpy(buffer, tty_driver.tty_line_buffer, bytes_to_copy);
  debugk("[TTY] PID %llu read %llu bytes\n", current_task->pid, bytes_to_copy);

  /* Consume only the bytes we delivered; shift remainder to the front.
   * dst < src so memcpy is safe (no overlap clobber on a left-shift). */
  uint64_t remaining = tty_driver.tty_line_buffer_size - bytes_to_copy;
  if (remaining > 0)
    memcpy(tty_driver.tty_line_buffer,
           tty_driver.tty_line_buffer + bytes_to_copy,
           remaining);
  tty_driver.tty_line_buffer_size = remaining;
  if (remaining == 0)
    tty_driver.buffer_ready = false;

  return (int64_t)bytes_to_copy;
}

int64_t tty_write(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  char *buf = (char *)buffer;
  for (uint64_t i = 0; i < size; i++) {
    printk("%c", buf[i]);
  }
  return size;
}

int tty_ioctl(struct file_t *file, unsigned long request, void *arg) {
  switch (request) {
    case TIOCGPGRP: {
      copy_to_user(arg, &tty_driver.foreground_pgid, sizeof(uint64_t));
      return 0;
    }
    case TIOCSPGRP: {
      uint64_t pgid;
      copy_from_user(&pgid, arg, sizeof(uint64_t));
      debugk("[TTY] Setting foreground PGID to %llu\n", pgid);
      tty_driver.foreground_pgid = pgid;
      return 0;
    }
    case TCSRAW:
      tty_driver.raw_mode = true;
      tty_reset_buffer();
      debugk("[TTY] Switched to raw mode\n");
      return 0;
    case TCSCANON:
      tty_driver.raw_mode = false;
      tty_reset_buffer();
      debugk("[TTY] Switched to canonical mode\n");
      return 0;
    default:
      return -EINVAL;
  }
}

struct tty_driver_t tty_driver;

void tty_init(void) {
  tty_driver.file_ops.read = tty_read;
  tty_driver.file_ops.write = tty_write;
  tty_driver.file_ops.ioctl = tty_ioctl;
  tty_driver.tty_line_buffer_size = 0;
  tty_driver.buffer_ready = false;
  tty_driver.foreground_pgid = 1;
  list_init(&tty_driver.wait_queue);
}

void tty_reset_buffer(void) {
  tty_driver.buffer_ready = false;
  tty_driver.tty_line_buffer_size = 0;
}

void tty_receive(char *buffer, uint64_t size) {
  debugk("[TTY] tty_receive called with size=%llu, buffer_ready=%d\n",
         size, tty_driver.buffer_ready);

  if (tty_driver.buffer_ready) {
    debugk("[TTY] Buffer already ready, ignoring input\n");
    return;
  }

  for (uint64_t i = 0; i < size; i++) {
    char c = buffer[i];
    debugk("[TTY] Received char: 0x%x ('%c')\n", c, c >= 32 && c < 127 ? c : '?');

    /* Ctrl-C always sends SIGINT regardless of mode. */
    if (c == 0x03) {
      debugk("[TTY] Ctrl-C detected, sending SIGINT to PGID %llu\n", tty_driver.foreground_pgid);
      if (!tty_driver.raw_mode)
        printk("^C\n");
      send_signal_to_pgid(tty_driver.foreground_pgid, SIGINT);
      tty_reset_buffer();
      continue;
    }

    if (tty_driver.raw_mode) {
      /* Raw mode: no echo, no processing — buffer the byte and wake the reader. */
      if (tty_driver.tty_line_buffer_size < 1024)
        tty_driver.tty_line_buffer[tty_driver.tty_line_buffer_size++] = c;
      tty_driver.buffer_ready = true;
      continue;
    }

    /* Canonical mode processing below. */

    // Handle backspace
    if (c == '\b' || c == 127) {
      if (tty_driver.tty_line_buffer_size > 0) {
        tty_driver.tty_line_buffer_size--;
        printk("\b \b");
      }
      continue;
    }

    if (tty_driver.tty_line_buffer_size >= 1024) {
      tty_driver.buffer_ready = true;
      break;
    }

    // Convert \r to \n for consistency
    if (c == '\r')
      c = '\n';

    printk("%c", c);
    tty_driver.tty_line_buffer[tty_driver.tty_line_buffer_size++] = c;

    if (c == '\n') {
      tty_driver.buffer_ready = true;
      break;
    }
  }

  if (tty_driver.buffer_ready) {
    debugk("[TTY] Buffer ready, waking up readers\n");
    wake_up(&tty_driver.wait_queue);
  }
}
