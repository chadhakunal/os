#ifndef PAGE_H
#define PAGE_H
#define DEFAULT_PAGE_SIZE 0x1000 // 4096 bytes

#include "types.h"

typedef struct page {
  uint64_t page_frame_id;
  bool is_kernel;
  bool is_zeroed;
  bool is_disk_cache;
  bool in_use;
  uint16_t refcount; /* COW share count: 1 = exclusively owned, >1 = shared */
  struct page *next_free_page;
} page_t;

typedef struct pages_metadata_struct {
  page_t *free_page_head;
  page_t *zero_page_head;
  page_t *page_list;
  uint64_t page_list_size;
  uint64_t total_pages;
  uint64_t pages_in_use;
} pages_metadata_struct_t;

extern pages_metadata_struct_t pages_metadata;

void init_page_allocator();
void print_pages_metadata();

/* Page allocation (returns physical address) */
void *get_page(bool is_kernel);
void *get_zero_page(bool is_kernel);
void free_page(void *p);

/* Refcount operations for COW */
void page_incref(void *phys);
void page_decref(void *phys); /* frees when refcount drops to 0 */
uint16_t page_get_refcount(void *phys);

void update_page_structs_to_vm();

void *_get_page_address_from_page(page_t *p);
page_t *_address_to_page(void *addr);

#endif
