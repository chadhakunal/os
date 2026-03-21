#include "platform.h"
#include "types.h"

#include "kernel/memory/page_allocator.h"
#include "kernel/memory/memory_info.h"

#include "kernel/panic.h"
#include "lib/printk/printk.h"

#define ALIGN_UP(x, a) (((uintptr_t)(x) + ((a) - 1)) & ~((uintptr_t)((a) - 1)))

pages_metadata_struct_t pages_metadata = {0};

bool in_kernel_space(uintptr_t start, uintptr_t end) {
  return start < memory_info.kernel_end && end > memory_info.kernel_start;
}

void init_page_allocator() {
  pages_metadata.pages_in_use = 0;
  pages_metadata.total_pages = memory_info.total_memory_size / DEFAULT_PAGE_SIZE;
  pages_metadata.page_list_size = pages_metadata.total_pages * sizeof(struct page);

  uintptr_t kernel_end = ALIGN_UP((uintptr_t)&_end, DEFAULT_PAGE_SIZE);
  page_t *last_free = NULL;
  memory_info.kernel_end = ALIGN_UP(kernel_end + pages_metadata.page_list_size, DEFAULT_PAGE_SIZE);
  pages_metadata.page_list = (page_t *)kernel_end;
  for (uint64_t i = 0; i < pages_metadata.total_pages; i++) {
    pages_metadata.page_list[i].page_frame_id = i;
    pages_metadata.page_list[i].is_zeroed = false;
    pages_metadata.page_list[i].is_disk_cache = false;
    pages_metadata.page_list[i].is_kernel = false;
    pages_metadata.page_list[i].in_use = false;
    pages_metadata.page_list[i].next_free_page = NULL;
    uintptr_t page_start = DEFAULT_PAGE_SIZE * i + memory_info.total_memory_base;
    uintptr_t page_end = page_start + DEFAULT_PAGE_SIZE;
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
}

void print_pages_metadata() {
  printk("Pages Metadata\n");
  printk("\tMemory Used for Page List: %lu KB\n", pages_metadata.page_list_size/1024);
  printk("\tTotal Pages: %lu\n", pages_metadata.total_pages);
  printk("\tPages in Use: %lu\n", pages_metadata.pages_in_use);
  printk("\tPage List Address: %lx\n", (uint64_t)pages_metadata.page_list);
  printk("\tFirst Free Page Frame: %lu\n", (uint64_t)pages_metadata.free_page_head->page_frame_id);
}

void *get_page(bool is_kernel) {
  page_t *first_free_page = pages_metadata.free_page_head;
  if (first_free_page == NULL) {
    panic("ATTEMPTED TO get_page, RAN OUT OF FREE PAGES");
  }
  pages_metadata.free_page_head = first_free_page->next_free_page;
  first_free_page->is_kernel = is_kernel;
  first_free_page->in_use = true;
  first_free_page->next_free_page = NULL;
  pages_metadata.pages_in_use++;
  return _get_page_address_from_page(first_free_page);
}

void* _get_page_address_from_page(page_t *p) {
  return (void *)(memory_info.total_memory_base + ((uintptr_t)p->page_frame_id * DEFAULT_PAGE_SIZE));
}

page_t* _address_to_page(void *addr) {
    uintptr_t offset = (uintptr_t)addr - memory_info.total_memory_base;

    uint64_t pfn = offset / DEFAULT_PAGE_SIZE;

    return &pages_metadata.page_list[pfn];
}

void free_page(void *p) {
  page_t *freed_page = _address_to_page(p);
  if (!freed_page->in_use) {
    panic("DOUBLE FREE DETECTED");
  }
  freed_page->in_use = false;
  freed_page->is_kernel = false;
  pages_metadata.pages_in_use--;
  freed_page->next_free_page = pages_metadata.free_page_head;
  pages_metadata.free_page_head = freed_page;
}
