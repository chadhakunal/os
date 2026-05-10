#include "kernel/signal_jump_point.h"
#include "kernel/memory/page_allocator.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "lib/string.h"
#include "kernel/panic.h"
#include "kernel/printk.h"

static void *signal_jump_point_page = NULL;

extern char __signal_jump_point_start[];
extern char __signal_jump_point_end[];

void init_signal_jump_point(void) {
  if (signal_jump_point_page != NULL) {
    return;
  }

  signal_jump_point_page = get_page(true);
  if (!signal_jump_point_page) {
    panic("Failed to allocate signal jump point page");
  }

  void *jump_point_virt = PHYS_TO_VIRT(signal_jump_point_page);
  size_t jump_point_size =
      __signal_jump_point_end - __signal_jump_point_start;

  if (jump_point_size > DEFAULT_PAGE_SIZE) {
    panic("Signal jump point code too large");
  }

  memcpy(jump_point_virt, __signal_jump_point_start, jump_point_size);

  printk("Signal jump point initialized at physical address %p, size %d bytes\n",
         signal_jump_point_page, jump_point_size);
}

void *get_signal_jump_point_page(void) { return signal_jump_point_page; }
