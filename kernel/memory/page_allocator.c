#define DEBUG 0
#include "platform.h"
#include "types.h"

#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_allocator.h"
#include "lib/printk/printk.h"

#include "kernel/panic.h"
#include "lib/printk/printk.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "arch/riscv64/trap.h"
#include "lib/string.h"

#define ALIGN_UP(x, a) (((uintptr_t)(x) + ((a) - 1)) & ~((uintptr_t)((a) - 1)))

pages_metadata_struct_t pages_metadata = {0};

bool in_kernel_space(uintptr_t start, uintptr_t end) {
  return start < memory_info.kernel_end && end > memory_info.kernel_start;
}

void init_page_allocator() {
  pages_metadata.pages_in_use = 0;
  pages_metadata.total_pages =
      memory_info.total_memory_size / DEFAULT_PAGE_SIZE;
  pages_metadata.page_list_size =
      pages_metadata.total_pages * sizeof(struct page);

  uintptr_t kernel_end = ALIGN_UP((uintptr_t)&_end, DEFAULT_PAGE_SIZE);
  page_t *last_free = NULL;
  memory_info.kernel_end =
      ALIGN_UP(kernel_end + pages_metadata.page_list_size, DEFAULT_PAGE_SIZE);
  pages_metadata.page_list = (page_t *)kernel_end;
  for (uint64_t i = 0; i < pages_metadata.total_pages; i++) {
    pages_metadata.page_list[i].page_frame_id = i;
    pages_metadata.page_list[i].is_zeroed = false;
    pages_metadata.page_list[i].is_disk_cache = false;
    pages_metadata.page_list[i].is_kernel = false;
    pages_metadata.page_list[i].in_use = false;
    pages_metadata.page_list[i].next_free_page = NULL;
    uintptr_t page_start =
        DEFAULT_PAGE_SIZE * i + memory_info.total_memory_base;
    uintptr_t page_end = page_start + DEFAULT_PAGE_SIZE;

    /* Mark page frame 0 and kernel pages as in-use */
    if (i == 0 || in_kernel_space(page_start, page_end) ||
        page_end <= memory_info.kernel_start) {
      pages_metadata.page_list[i].is_kernel = true;
      pages_metadata.page_list[i].in_use = true;
      pages_metadata.pages_in_use++;
    } else {
      if (last_free == NULL) {
        pages_metadata.free_page_head = &pages_metadata.page_list[i];
        last_free = &pages_metadata.page_list[i];
      } else {
        last_free->next_free_page = &pages_metadata.page_list[i];
        last_free = &pages_metadata.page_list[i];
      }
    }
  }

  // Zero all free pages and move them to zero list
  pages_metadata.zero_page_head = pages_metadata.free_page_head;
  pages_metadata.free_page_head = NULL;

  debugk("init_page_allocator: starting to zero free pages\n");
  page_t *cur = pages_metadata.zero_page_head;
  uint64_t pages_zeroed = 0;
  while (cur) {
    void *page_addr = _get_page_address_from_page(cur);
    memset(page_addr, 0, DEFAULT_PAGE_SIZE);
    cur->is_zeroed = true;
    pages_zeroed++;
    if (pages_zeroed % 1000 == 0) {
      debugk("init_page_allocator: zeroed %llu pages so far\n", pages_zeroed);
    }
    cur = cur->next_free_page;
  }
  debugk("init_page_allocator: finished zeroing %llu pages\n", pages_zeroed);
}

void print_pages_metadata() {
  printk("Pages Metadata\n");
  printk("\tMemory Used for Page List: %lu KB\n",
         pages_metadata.page_list_size / 1024);
  printk("\tTotal Pages: %lu\n", pages_metadata.total_pages);
  printk("\tPages in Use: %lu\n", pages_metadata.pages_in_use);
  printk("\tPage List Address: %lx\n", (uint64_t)pages_metadata.page_list);
  if (pages_metadata.free_page_head) {
    printk("\tFirst Free Page Frame: %lu\n",
           (uint64_t)pages_metadata.free_page_head->page_frame_id);
  }
  if (pages_metadata.zero_page_head) {
    printk("\tFirst Zero Page Frame: %lu\n",
           (uint64_t)pages_metadata.zero_page_head->page_frame_id);
  }
}

// Returns physical address of allocated page
void *get_page(bool is_kernel) {
  page_t *first_free_page = pages_metadata.free_page_head;

  // If no pages in regular free list, fall back to zero list
  if (first_free_page == NULL) {
    first_free_page = pages_metadata.zero_page_head;
    if (first_free_page == NULL) {
      panic("ATTEMPTED TO get_page, RAN OUT OF FREE PAGES");
    }
    pages_metadata.zero_page_head = first_free_page->next_free_page;
  } else {
    pages_metadata.free_page_head = first_free_page->next_free_page;
  }

  first_free_page->is_kernel = is_kernel;
  first_free_page->in_use = true;
  first_free_page->is_zeroed = false;
  first_free_page->next_free_page = NULL;
  pages_metadata.pages_in_use++;
  return _get_page_address_from_page(first_free_page);
}

// Returns physical address of allocated zeroed page, or NULL if none available
void *get_zero_page(bool is_kernel) {
  page_t *first_zero_page = pages_metadata.zero_page_head;
  if (first_zero_page == NULL) {
    return NULL;
  }
  pages_metadata.zero_page_head = first_zero_page->next_free_page;
  first_zero_page->is_kernel = is_kernel;
  first_zero_page->in_use = true;
  first_zero_page->is_zeroed = true;
  first_zero_page->next_free_page = NULL;
  pages_metadata.pages_in_use++;
  return _get_page_address_from_page(first_zero_page);
}

void *_get_page_address_from_page(page_t *p) {
  return (void *)(memory_info.total_memory_base +
                  ((uintptr_t)p->page_frame_id * DEFAULT_PAGE_SIZE));
}

page_t *_address_to_page(void *addr) {
  uintptr_t offset = (uintptr_t)addr - memory_info.total_memory_base;

  uint64_t pfn = offset / DEFAULT_PAGE_SIZE;

  return &pages_metadata.page_list[pfn];
}

// Accepts physical address
void free_page(void *p) {
  page_t *freed_page = _address_to_page(p);
  if (!freed_page->in_use) {
    panic("DOUBLE FREE DETECTED");
  }
  debugk("free_page: phys=%p, pages_in_use: %llu -> %llu\n",
         p, pages_metadata.pages_in_use, pages_metadata.pages_in_use - 1);
  freed_page->in_use = false;
  freed_page->is_kernel = false;
  pages_metadata.pages_in_use--;

  // Zero the page and add to zero list
  // Disable interrupts during memset to prevent race conditions:
  // If a timer interrupt fires while we're zeroing memory, and the interrupt
  // handler tries to access memory (e.g., for printing), it could hit corrupted
  // page table entries or partially-zeroed pages
  void *virt_page = PHYS_TO_VIRT(p);

  // Check if interrupts are already disabled
  uint64_t sstatus;
  asm volatile("csrr %0, sstatus" : "=r"(sstatus));

  disable_interrupts();
  memset(virt_page, 0, DEFAULT_PAGE_SIZE);
  freed_page->is_zeroed = true;

  // Verify interrupts are actually disabled
  asm volatile("csrr %0, sstatus" : "=r"(sstatus));
  if (sstatus & SSTATUS_SIE) {
    panic("free_page: BUG! Interrupts still enabled after disable_interrupts()!");
  }

  enable_interrupts();

  freed_page->next_free_page = pages_metadata.zero_page_head;
  pages_metadata.zero_page_head = freed_page;
}

void convert_free_list_to_virtual() {
  pages_metadata.page_list = PHYS_TO_VIRT(pages_metadata.page_list);

  if (pages_metadata.free_page_head) {
    pages_metadata.free_page_head = PHYS_TO_VIRT(pages_metadata.free_page_head);
    page_t *cur = pages_metadata.free_page_head;
    while (cur) {
      if (cur->next_free_page)
        cur->next_free_page = PHYS_TO_VIRT(cur->next_free_page);
      cur = cur->next_free_page;
    }
  }

  if (pages_metadata.zero_page_head) {
    pages_metadata.zero_page_head = PHYS_TO_VIRT(pages_metadata.zero_page_head);
    page_t *cur = pages_metadata.zero_page_head;
    while (cur) {
      if (cur->next_free_page)
        cur->next_free_page = PHYS_TO_VIRT(cur->next_free_page);
      cur = cur->next_free_page;
    }
  }
}

void update_page_structs_to_vm() {
  convert_free_list_to_virtual();
}
