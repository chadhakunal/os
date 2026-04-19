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

/* Boot-time version: accesses physical address directly */
page_table_t *boot_allocate_page_table() {
  page_table_t *pt = (page_table_t *)get_page(true);
  memset(pt, 0, DEFAULT_PAGE_SIZE);
  return pt;
}

/* Post-boot version: accesses via PHYS mapping, returns physical address */
page_table_t *allocate_page_table() {
  page_table_t *pt_phys = (page_table_t *)get_page(true);
  page_table_t *pt_virt = (page_table_t *)PHYS_TO_VIRT(pt_phys);
  memset(pt_virt, 0, DEFAULT_PAGE_SIZE);
  return pt_phys;  /* PTEs need physical addresses */
}

void allocate_root_page_table() {
  root_page_table = boot_allocate_page_table();
  if (!root_page_table)
    panic("FAILED TO ALLOCATE ROOT PAGE TABLE!");
}

/* Boot-time version: uses physical addresses directly (identity mapping active) */
void boot_map_page(page_table_t *pt, uint64_t va, uint64_t pa) {
  uint64_t pt1_idx = PT1_OFFSET(va); // VPN[2]
  uint64_t pt2_idx = PT2_OFFSET(va); // VPN[1]
  uint64_t pt3_idx = PT3_OFFSET(va); // VPN[0]

  page_table_t *pt2;
  page_table_t *pt3;

  // Root table (pt1 == pt) is indexed by VPN[2]
  if (pt->page_table_entries[pt1_idx] == 0) {
    pt2 = boot_allocate_page_table();
    if (!pt2)
      panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
    pt->page_table_entries[pt1_idx] =
        PTE_ADDR(pt2) | PTE_VALID | PTE_TABLE;
  } else {
    pt2 = (page_table_t *)PTE_DECODE(
        pt->page_table_entries[pt1_idx]);
  }

  if (pt2->page_table_entries[pt2_idx] == 0) {
    pt3 = boot_allocate_page_table();
    if (!pt3)
      panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
    pt2->page_table_entries[pt2_idx] = PTE_ADDR(pt3) | PTE_VALID | PTE_TABLE;
  } else {
    pt3 = (page_table_t *)PTE_DECODE(pt2->page_table_entries[pt2_idx]);
  }

  pt3->page_table_entries[pt3_idx] =
      PTE_ADDR(pa) | PTE_VALID | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D;
}

/* Post-boot version: uses PHYS_TO_VIRT to access page tables */
void map_page(page_table_t *pt, uint64_t va, uint64_t pa, uint64_t pte_flags) {
  uint64_t pt1_idx = PT1_OFFSET(va); // VPN[2]
  uint64_t pt2_idx = PT2_OFFSET(va); // VPN[1]
  uint64_t pt3_idx = PT3_OFFSET(va); // VPN[0]

  page_table_t *pt2;
  page_table_t *pt3;

  // Root table (pt1 == pt) is indexed by VPN[2]
  if (pt->page_table_entries[pt1_idx] == 0) {
    page_table_t *pt2_phys = allocate_page_table(); /* Returns physical address */
    if (!pt2_phys)
      panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
    pt->page_table_entries[pt1_idx] =
        PTE_ADDR(pt2_phys) | PTE_VALID | PTE_TABLE;
    pt2 = (page_table_t *)PHYS_TO_VIRT(pt2_phys); /* Convert to virtual for access */
  } else {
    pt2 = (page_table_t *)PHYS_TO_VIRT(
        PTE_DECODE(pt->page_table_entries[pt1_idx]));
  }

  if (pt2->page_table_entries[pt2_idx] == 0) {
    page_table_t *pt3_phys = allocate_page_table(); /* Returns physical address */
    if (!pt3_phys)
      panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
    pt2->page_table_entries[pt2_idx] = PTE_ADDR(pt3_phys) | PTE_VALID | PTE_TABLE;
    pt3 = (page_table_t *)PHYS_TO_VIRT(pt3_phys); /* Convert to virtual for access */
  } else {
    pt3 = (page_table_t *)PHYS_TO_VIRT(
        PTE_DECODE(pt2->page_table_entries[pt2_idx]));
  }

  // Set leaf PTE: VALID + A (accessed) always set, pte_flags from caller
  // PTE_D (dirty) starts at 0 - hardware sets it on write
  pt3->page_table_entries[pt3_idx] = PTE_ADDR(pa) | PTE_VALID | PTE_A | pte_flags;
}

bool page_table_empty(page_table_t *pt) {
  for (int i = 0; i < 512; i++) {
    if (pt->page_table_entries[i] & PTE_VALID)
      return false;
  }
  return true;
}

void unmap_page(page_table_t *pt, uint64_t va) {
  uint64_t pt1_idx = PT1_OFFSET(va);
  uint64_t pt2_idx = PT2_OFFSET(va);
  uint64_t pt3_idx = PT3_OFFSET(va);
  if (!(pt->page_table_entries[pt1_idx] & PTE_VALID))
    return;
  page_table_t *pt2 =
    (page_table_t *)PHYS_TO_VIRT(
        PTE_DECODE(pt->page_table_entries[pt1_idx]));

  if (!(pt2->page_table_entries[pt2_idx] & PTE_VALID))
    return;
  page_table_t *pt3 =
    (page_table_t *)PHYS_TO_VIRT(
        PTE_DECODE(pt2->page_table_entries[pt2_idx]));

  /* clear leaf entry */
  pt3->page_table_entries[pt3_idx] = 0;
  /* free L0 table if empty */
  if (page_table_empty(pt3)) {
    //free_page((void *)VIRT_TO_PHYS(pt3));
    pt2->page_table_entries[pt2_idx] = 0;
  }

  /* free L1 table if empty */
  if (page_table_empty(pt2)) {
    //free_page((void *) VIRT_TO_PHYS(pt2));
    pt->page_table_entries[pt1_idx] = 0;
  }
}

void unmap_pages(page_table_t *pt, uint64_t virtual_memory_start, uint64_t virtual_memory_end) {
  for (uint64_t iter = 0; iter < virtual_memory_end-virtual_memory_start;
       iter += DEFAULT_PAGE_SIZE) {
    uint64_t va = iter + virtual_memory_start;
    unmap_page(pt, va);
  }
}

/* Boot-time version */
void boot_map_pages(page_table_t *pt, uint64_t physical_memory_start, uint64_t physical_memory_end,
                uint64_t virtual_memory_start) {
  for (uint64_t iter = 0; iter < physical_memory_end - physical_memory_start;
       iter += DEFAULT_PAGE_SIZE) {
    uint64_t pa = iter + physical_memory_start;
    uint64_t va = iter + virtual_memory_start;
    boot_map_page(pt, va, pa);
  }
}

/* Post-boot version */
void map_pages(page_table_t *pt, uint64_t physical_memory_start, uint64_t physical_memory_end,
                uint64_t virtual_memory_start, uint64_t pte_flags) {
  for (uint64_t iter = 0; iter < physical_memory_end - physical_memory_start;
       iter += DEFAULT_PAGE_SIZE) {
    uint64_t pa = iter + physical_memory_start;
    uint64_t va = iter + virtual_memory_start;
    map_page(pt, va, pa, pte_flags);
  }
}

void map_mmio() {
  boot_map_pages(root_page_table, 0x0, memory_info.total_memory_base, MMIO_VIRTUAL_MEMORY_BASE);
}

void map_phys() {
  boot_map_pages(root_page_table, memory_info.total_memory_base,
                  memory_info.total_memory_base + memory_info.total_memory_size,
                  PHYS_VIRTUAL_MEMORY_BASE);
}

void map_kernel() {
  boot_map_pages(root_page_table, memory_info.kernel_start, memory_info.kernel_end,
             KERNEL_VIRTUAL_MEMORY_BASE);
}

void map_identity() {
  /* Only map the kernel region identity-mapped.
     For Sv39, this covers the firmware and kernel code needed for MMU
     activation. The full 128MB doesn't need identity mapping once we're past
     boot. */
  boot_map_pages(root_page_table, memory_info.kernel_start, memory_info.kernel_end,
             memory_info.kernel_start);
}

void unmap_identity() {
  unmap_pages(root_page_table, memory_info.kernel_start, memory_info.kernel_end);
}

void remove_identity_mapping() {
  unmap_identity();
  asm volatile("sfence.vma zero, zero" ::: "memory");
}

void init_kernel_page_mapping() {
  allocate_root_page_table();
  map_identity();
  map_kernel();
  map_phys();  /* Map all physical RAM for accessing page tables and other phys mem */

  /* Map UART device for MMIO access after MMU is enabled */
  uint64_t uart_phys = (uint64_t)uart_get_base(); /* align to page */
  uart_phys &= ~(DEFAULT_PAGE_SIZE - 1);
  uint64_t uart_virt = MMIO_VIRTUAL_MEMORY_BASE + uart_phys;
  boot_map_page(root_page_table, uart_virt, uart_phys);

  enable_virtual_memory((uint64_t)root_page_table);
  uint64_t offset = KERNEL_VIRT_OFFSET;

  uint64_t old_sp;
  asm volatile("mv %0, sp" : "=r"(old_sp));

  asm volatile("la t0, 1f\n"
               "add t0, t0, %[off]\n"
               "jr t0\n"

               "1:\n"

               /* fix stack pointer */
               "add sp, sp, %[off]\n"

               :
               : [off] "r"(offset)
               : "t0", "memory");

  uint64_t new_sp;
  asm volatile("mv %0, sp" : "=r"(new_sp));
  root_page_table = PHYS_TO_VIRT(root_page_table);
  update_page_structs_to_vm();
}

page_table_t *init_new_page_table() {
  page_table_t *new_pt_phys = (page_table_t *)get_page(true);
  page_table_t *new_pt_virt = (page_table_t *)PHYS_TO_VIRT(new_pt_phys);

  memset(new_pt_virt, 0, DEFAULT_PAGE_SIZE);

  for (uint64_t i = 256; i < 512; i++) {
    new_pt_virt->page_table_entries[i] = root_page_table->page_table_entries[i];
  }

  return new_pt_phys;  /* Return physical address for satp */
}
