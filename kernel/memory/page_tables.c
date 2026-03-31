#include "kernel/panic.h"
#include "platform.h"
#include "types.h"

#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/memory/page_tables.h"

#include "kernel/drivers/uart.h"
#include "lib/printk/printk.h"
#include "lib/string.h"
#include "virtual_memory_init.h"

page_table_t *root_page_table = NULL;

page_table_t *allocate_page_table() {
  page_table_t *pt = (page_table_t *)get_page(true);
  memset(pt, 0, DEFAULT_PAGE_SIZE);
  return pt;
};

void allocate_root_page_table() {
  root_page_table = allocate_page_table();
  if (!root_page_table)
    panic("FAILED TO ALLOCATE ROOT PAGE TABLE!");
}

void create_page_table_entry(uint64_t va, uint64_t pa) {
  uint64_t pt1_idx = PT1_OFFSET(va); // VPN[2]
  uint64_t pt2_idx = PT2_OFFSET(va); // VPN[1]
  uint64_t pt3_idx = PT3_OFFSET(va); // VPN[0]

  page_table_t *pt2;
  page_table_t *pt3;

  // Root table (pt1 == root_page_table) is indexed by VPN[2]
  if (root_page_table->page_table_entries[pt1_idx] == 0) {
    pt2 = allocate_page_table();
    if (!pt2)
      panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
    root_page_table->page_table_entries[pt1_idx] =
        PTE_ADDR(pt2) | PTE_VALID | PTE_TABLE;
  } else {
    pt2 = (page_table_t *)PTE_DECODE(
        root_page_table->page_table_entries[pt1_idx]);
  }

  if (pt2->page_table_entries[pt2_idx] == 0) {
    pt3 = allocate_page_table();
    if (!pt3)
      panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
    pt2->page_table_entries[pt2_idx] = PTE_ADDR(pt3) | PTE_VALID | PTE_TABLE;
  } else {
    pt3 = (page_table_t *)PTE_DECODE(pt2->page_table_entries[pt2_idx]);
  }

  pt3->page_table_entries[pt3_idx] =
      PTE_ADDR(pa) | PTE_VALID | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D;
}

bool page_table_empty(page_table_t *pt) {
  for (int i = 0; i < 512; i++) {
    if (pt->page_table_entries[i] & PTE_VALID)
      return false;
  }
  return true;
}

void remove_page_table_entry(uint64_t va) {
  uint64_t pt1_idx = PT1_OFFSET(va);
  uint64_t pt2_idx = PT2_OFFSET(va);
  uint64_t pt3_idx = PT3_OFFSET(va);

  if (!(root_page_table->page_table_entries[pt1_idx] & PTE_VALID))
    return;

page_table_t *pt2 =
    (page_table_t *)PHYS_TO_VIRT(
        PTE_DECODE(root_page_table->page_table_entries[pt1_idx]));

  if (!(pt2->page_table_entries[pt2_idx] & PTE_VALID))
    return;

page_table_t *pt3 =
    (page_table_t *)PHYS_TO_VIRT(
        PTE_DECODE(pt2->page_table_entries[pt2_idx]));

  /* clear leaf entry */
  pt3->page_table_entries[pt3_idx] = 0;

  /* free L0 table if empty */
  if (page_table_empty(pt3)) {
    free_page((void *)VIRT_TO_PHYS(pt3));
    pt2->page_table_entries[pt2_idx] = 0;
  }

  /* free L1 table if empty */
  if (page_table_empty(pt2)) {
    free_page((void *) VIRT_TO_PHYS(pt2));
    root_page_table->page_table_entries[pt1_idx] = 0;
  }
}

void unmap_region(uint64_t virtual_memory_start, uint64_t virtual_memory_end) {
  for (uint64_t iter = 0; iter < virtual_memory_end-virtual_memory_start;
       iter += DEFAULT_PAGE_SIZE) {
    uint64_t va = iter + virtual_memory_start;
    remove_page_table_entry(va);
  }
}

void map_region(uint64_t physical_memory_start, uint64_t physical_memory_end,
                uint64_t virtual_memory_start) {
  for (uint64_t iter = 0; iter < physical_memory_end - physical_memory_start;
       iter += DEFAULT_PAGE_SIZE) {
    uint64_t pa = iter + physical_memory_start;
    uint64_t va = iter + virtual_memory_start;
    create_page_table_entry(va, pa);
  }
}
void map_mmio() {
  map_region(0x0, memory_info.total_memory_base, MMIO_VIRTUAL_MEMORY_BASE);
}

void map_phys() {
  map_region(memory_info.total_memory_base, memory_info.total_memory_size,
             PHYS_VIRTUAL_MEMORY_BASE);
}

void map_kernel() {
  map_region(memory_info.kernel_start, memory_info.kernel_end,
             KERNEL_VIRTUAL_MEMORY_BASE);
}

void map_identity() {
  /* Only map the kernel region identity-mapped.
     For Sv39, this covers the firmware and kernel code needed for MMU
     activation. The full 128MB doesn't need identity mapping once we're past
     boot. */
  map_region(memory_info.kernel_start, memory_info.kernel_end,
             memory_info.kernel_start);
}

void init_page_mapping() {
  if (!root_page_table)
    allocate_root_page_table();
  map_identity();
  map_kernel();

  /* Map UART device for MMIO access after MMU is enabled */
  uint64_t uart_phys = (uint64_t)uart_get_base(); /* align to page */
  uart_phys &= ~(DEFAULT_PAGE_SIZE - 1);
  uint64_t uart_virt = MMIO_VIRTUAL_MEMORY_BASE;
  printk("MMIO base = %llx\n", 0xFFFFFFD000000000ULL);
  printk("uart_virt: %llx, uart_phys: %llx\n", uart_virt, uart_phys);
  create_page_table_entry(uart_virt, uart_phys);
  create_page_table_entry(uart_phys, uart_phys);
  // if (platform.uart.base != 0) {
  //   /* Map one page containing the UART device */
  //   printk("in platform uart base != 0\n");
  //   create_page_table_entry(uart_virt, uart_phys);
  //   create_page_table_entry(uart_phys, uart_phys);
  // }

  printk("About to enable virtual mem\n");
  enable_virtual_memory((uint64_t)root_page_table);
  printk("virt mem enabled\n");
  uint64_t offset = KERNEL_VIRT_OFFSET;

  asm volatile("la t0, 1f\n"
               "add t0, t0, %[off]\n"
               "jr t0\n"

               "1:\n"

               /* fix stack pointer */
               "add sp, sp, %[off]\n"

               :
               : [off] "r"(offset)
               : "t0", "memory");

  printk("after moving kernel\n");
  update_page_structs_to_vm();
  printk("Updated paging to virtual\n");
}
