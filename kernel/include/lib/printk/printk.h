#ifndef PRINTK_H
#define PRINTK_H

#include "types.h"
#include <stdarg.h>

void printk(const char *fmt, ...);
void vprintk(const char *fmt, va_list args);

/* Kernel log ring buffer — readable via /proc/kmsg or dmesg. */
#define KLOG_SIZE (64 * 1024)
size_t klog_read(char *buf, size_t size, size_t offset);
size_t klog_len(void);

// Include Debug in macro before including printk and set to 1 like so "#define DEBUG 1\n#include "lib/printk/printk.h"
#ifndef DEBUG
#define DEBUG 0
#endif

#if DEBUG
#define debugk(fmt, ...) printk("[DEBUG] %s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define debugk(fmt, ...) ((void)0)
#endif

#endif
