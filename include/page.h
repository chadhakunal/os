#ifndef PAGE_H
#define PAGE_H
#define PAGE_SIZE 4096

#include "types.h"

struct page {
  uint64_t page_frame_id;
  bool is_kernel;
  bool is_zeroed;
  bool is_disk_cache;
  bool in_use;
  struct page *next_free_page;
};

struct pages_metadata_struct {
  struct page *free_page_head;
  struct page *page_list;
  uint64_t total_pages;
  uint64_t pages_in_use;
};

extern volatile struct pages_metadata_struct pages_metadata;
volatile struct pages_metadata_struct *init_paging();
bool in_kernel_space(uintptr_t start, uintptr_t end);
void print_pages_metadata();

void *get_free_page(bool is_kernel);
void free_page(void *p); // Releases page that contains the address

void *_get_page_address_from_page(struct page *p);
struct page *_address_to_page(void *addr);

#endif
