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
  struct page *next_free_page;
} page_t;

typedef struct pages_metadata_struct {
  page_t *free_page_head;
  page_t *page_list;
  uint64_t page_list_size;
  uint64_t total_pages;
  uint64_t pages_in_use;
} pages_metadata_struct_t;

extern pages_metadata_struct_t pages_metadata;

void init_page_allocator();
void print_pages_metadata();

/* Boot-time functions (use physical addresses) */
void *boot_get_page(bool is_kernel);
void boot_free_page(void *p);

/* Post-boot functions (use virtual addresses via PHYS mapping) */
void *get_page(bool is_kernel);
void free_page(void *p);

void update_page_structs_to_vm();

void *_get_page_address_from_page(page_t *p);
page_t *_address_to_page(void *addr);

#endif
