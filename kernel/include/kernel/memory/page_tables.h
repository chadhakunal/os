#ifndef PAGE_TABLES_H
#define PAGE_TABLES_H

#define NUM_PTE 512

typedef uint64_t pte_t;

typedef struct page_table {
  pte_t page_table_entries[NUM_PTE];
} page_table_t;

extern page_table_t *root_page_table;

void init_kernel_page_mapping();
void remove_identity_mapping();

/* Post-boot functions (use PHYS_TO_VIRT for page table access) */
void map_page(page_table_t *pt, uint64_t va, uint64_t pa, uint64_t pte_flags);
void map_pages(page_table_t *pt, uint64_t pa_start, uint64_t pa_end, uint64_t va_start, uint64_t pte_flags);
void unmap_page(page_table_t *pt, uint64_t va);
void unmap_pages(page_table_t *pt, uint64_t va_start, uint64_t va_end);

page_table_t *init_new_page_table();

#endif
