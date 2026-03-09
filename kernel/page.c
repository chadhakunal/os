#include "page.h"
#include "kernel/memory.h"
#include "kernel/drivers/uart.h"
#include "platform.h"
#include "types.h"
#include "kernel/panic.h"

#define ALIGN_UP(x, a) (((uintptr_t)(x) + ((a) - 1)) & ~((uintptr_t)((a) - 1)))

volatile struct pages_metadata_struct pages_metadata = {0};

volatile struct pages_metadata_struct *init_paging() {
  pages_metadata.pages_in_use = 0;
  pages_metadata.total_pages = kernel_memory.total_memory_size / PAGE_SIZE;
  uint64_t page_list_size = pages_metadata.total_pages * sizeof(struct page);

  uintptr_t kernel_end = ALIGN_UP((uintptr_t)&_end, PAGE_SIZE);
  struct page *last_free = NULL;
  kernel_memory.kernel_end = ALIGN_UP(kernel_end + page_list_size, PAGE_SIZE);
  pages_metadata.page_list = (struct page *)kernel_end;
  for (uint64_t i = 0; i < pages_metadata.total_pages; i++) {
    pages_metadata.page_list[i].page_frame_id = i;
    pages_metadata.page_list[i].is_zeroed = false;
    pages_metadata.page_list[i].is_disk_cache = false;
    pages_metadata.page_list[i].is_kernel = false;
    pages_metadata.page_list[i].in_use = false;
    pages_metadata.page_list[i].next_free_page = NULL;
    uintptr_t page_start = PAGE_SIZE * i + kernel_memory.total_memory_base;
    uintptr_t page_end = page_start + PAGE_SIZE;
    if (in_kernel_space(page_start, page_end)) {
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
  return &pages_metadata;
}

bool in_kernel_space(uintptr_t start, uintptr_t end) {
  return start < kernel_memory.kernel_end &&
         end > kernel_memory.kernel_start;
}

void print_pages_metadata() {
  uart_print("Pages Metadata\n");
  uart_print("\tTotal Pages: ");
  uart_print_long_int(pages_metadata.total_pages);
  uart_print("\tPages in Use: ");
  uart_print_long_int(pages_metadata.pages_in_use);
  uart_print("\tPage List Address: ");
  uart_print_hex((uint64_t)pages_metadata.page_list);
  uart_print("\tFirst Free Page: ");
  uart_print_long_int((uint64_t)pages_metadata.free_page_head->page_frame_id);
}

void *get_free_page(bool is_kernel) {
  struct page *first_free_page = pages_metadata.free_page_head;
  if (first_free_page == NULL) {
    panic("ATTEMPTED TO get_free_page, RAN OUT OF FREE PAGES");
  }
  pages_metadata.free_page_head = first_free_page->next_free_page;
  first_free_page->is_kernel = is_kernel;
  first_free_page->in_use = true;
  first_free_page->next_free_page = NULL;
  pages_metadata.pages_in_use++;
  return _get_page_address_from_page(first_free_page);
}

void *_get_page_address_from_page(struct page *p) {
  return (void *)(kernel_memory.total_memory_base + ((uintptr_t)p->page_frame_id * PAGE_SIZE));
}

struct page *_address_to_page(void *addr) {
    uintptr_t offset = (uintptr_t)addr - kernel_memory.total_memory_base;

    uint64_t pfn = offset / PAGE_SIZE;

    return &pages_metadata.page_list[pfn];
}

void free_page(void *p) {
  struct page *freed_page = _address_to_page(p);
  if (!freed_page->in_use) {
    panic("DOUBLE FREE DETECTED");
  }
  freed_page->in_use = false;
  freed_page->is_kernel = false;
  pages_metadata.pages_in_use--;
  freed_page->next_free_page = pages_metadata.free_page_head;
  pages_metadata.free_page_head = freed_page;
}
