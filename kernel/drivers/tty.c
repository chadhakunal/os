#include "kernel/drivers/tty.h"
#include "lib/printk/printk.h"
#include "kernel/task/schedule.h"
#include "kernel/task/task.h"
#include "lib/string.h"

int64_t tty_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  while (!tty_driver.buffer_ready) {
    list_append(&tty_driver.wait_queue, &current_task->wait_list);
    current_task->state = TASK_BLOCKED;
    current_task->wait_reason = WAIT_IO;
    schedule();
  }

  uint64_t bytes_to_copy = tty_driver.tty_line_buffer_size < size ?
                           tty_driver.tty_line_buffer_size : size;

  memcpy(buffer, tty_driver.tty_line_buffer, bytes_to_copy);

  int64_t bytes_read = tty_driver.tty_line_buffer_size;

  tty_reset_buffer();

  return bytes_read;
}

int64_t tty_write(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  char *buf = (char *)buffer;
  for (uint64_t i = 0; i < size; i++) {
    printk("%c", buf[i]);
  }
  return size;
}

struct tty_driver_t tty_driver;

void tty_init(void) {
  tty_driver.file_ops.read = tty_read;
  tty_driver.file_ops.write = tty_write;
  tty_driver.tty_line_buffer_size = 0;
  tty_driver.buffer_ready = false;
  list_init(&tty_driver.wait_queue);
}

void tty_reset_buffer(void) {
  tty_driver.buffer_ready = false;
  tty_driver.tty_line_buffer_size = 0;
}

void tty_receive(char *buffer, uint64_t size) {
  if (tty_driver.buffer_ready) {
    return;
  }

  for (uint64_t i = 0; i < size; i++) {
    char c = buffer[i];

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

    printk("%c", c);

    tty_driver.tty_line_buffer[tty_driver.tty_line_buffer_size++] = c;

    if (c == '\n') {
      tty_driver.buffer_ready = true;
      break;
    }
  }

  if (tty_driver.buffer_ready) {
    wake_up(&tty_driver.wait_queue);
  }
}
