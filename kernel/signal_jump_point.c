#define DEBUG 0
#include "kernel/signal_jump_point.h"
#include "kernel/memory/page_allocator.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "lib/string.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"

static void *signal_jump_point_page = NULL;

// RISC-V trampoline: li a7, 139 (SYS_rt_sigreturn); ecall; j .
static const uint8_t trampoline[] = {
    0x93, 0x08, 0xB0, 0x08,  // li a7, 139
    0x73, 0x00, 0x00, 0x00,  // ecall
    0x6F, 0x00, 0x00, 0x00,  // j .
};

void init_signal_jump_point(void) {
  if (signal_jump_point_page != NULL) {
    return;
  }

  signal_jump_point_page = get_page(true);
  if (!signal_jump_point_page) {
    panic("Failed to allocate signal jump point page");
  }

  void *jump_point_virt = PHYS_TO_VIRT(signal_jump_point_page);
  memcpy(jump_point_virt, trampoline, sizeof(trampoline));

  uint32_t *instructions = (uint32_t *)jump_point_virt;
  printk("Signal jump point initialized at physical address %p, first_insn=0x%08x\n",
         signal_jump_point_page, instructions[0]);
}

void *get_signal_jump_point_page(void) { return signal_jump_point_page; }
