#ifndef PAGE_TABLES_H
#define PAGE_TABLES_H

#define NUM_PTE 512

typedef uint64_t pte_t;

typedef struct page_table {
  pte_t page_table_entries[NUM_PTE];
} page_table_t;

extern page_table_t *root_page_table;

void allocate_root_page_table();

page_table_t *allocate_page_table();
void create_page_table_entry(uint64_t va, uint64_t pa);
void remove_page_table_entry(uint64_t pa);

void init_page_mapping();

#endif
