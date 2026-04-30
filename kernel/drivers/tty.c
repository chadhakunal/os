#include "kernel/drivers/tty.h"
#include "lib/printk/printk.h"

int64_t tty_read(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  // TODO: Implement TTY read (get input from UART)
  return 0;
}

int64_t tty_write(struct file_t *file, uint64_t offset, void *buffer, uint64_t size) {
  char *buf = (char *)buffer;
  // Write each character directly to UART without null-terminating
  for (uint64_t i = 0; i < size; i++) {
    printk("%c", buf[i]);
  }
  return size;
}

struct tty_driver_t tty_driver;

void tty_init(void) {
  tty_driver.file_ops.read = tty_read;
  tty_driver.file_ops.write = tty_write;
}
