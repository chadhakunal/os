#include "types.h"
#include "platform.h"
#include "kernel/panic.h"

#include "kernel/memory/page_allocator.h"
#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_tables.h"

#include "virtual_memory.h"
#include "lib/string.h"

page_table_t* root_page_table = NULL;

page_table_t* allocate_page_table() {
    page_table_t* pt = (page_table_t*)get_page(true);
    memset(pt, 0, DEFAULT_PAGE_SIZE);
    return pt;
};

void allocate_root_page_table() {
    root_page_table = allocate_page_table();
    if(!root_page_table) panic("FAILED TO ALLOCATE ROOT PAGE TABLE!");
}

// TODO: The flags need to be generalized across Architectures (not sure how to yet)
void create_page_table_entry(uint64_t pa) {
    uint64_t pt0_idx = PT0_OFFSET(pa);
    uint64_t pt1_idx = PT1_OFFSET(pa);
    uint64_t pt2_idx = PT2_OFFSET(pa);
    uint64_t pt3_idx = PT3_OFFSET(pa);

    page_table_t* pt1;
    page_table_t* pt2;
    page_table_t* pt3;
    
    if(root_page_table->page_table_entries[pt0_idx] == 0) {
        pt1 = allocate_page_table();
        if(!pt1) panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
        root_page_table->page_table_entries[pt0_idx] = PTE_ADDR(pt1) | PTE_VALID | PTE_TABLE;
    } else {
        pt1 = (page_table_t*)PTE_DECODE(root_page_table->page_table_entries[pt0_idx]);
    }

    if(pt1->page_table_entries[pt1_idx] == 0) {
        pt2 = allocate_page_table();
        if(!pt2) panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
        pt1->page_table_entries[pt1_idx] = PTE_ADDR(pt2) | PTE_VALID | PTE_TABLE;
    } else {
        pt2 = (page_table_t*)PTE_DECODE(pt1->page_table_entries[pt1_idx]);
    }

    if(pt2->page_table_entries[pt2_idx] == 0) {
        pt3 = allocate_page_table();
        if(!pt3) panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
        pt2->page_table_entries[pt2_idx] = PTE_ADDR(pt3) | PTE_VALID | PTE_TABLE;
    } else {
        pt3 = (page_table_t*)PTE_DECODE(pt2->page_table_entries[pt2_idx]);
    }

    pt3->page_table_entries[pt3_idx] = PTE_ADDR(pa) | PTE_VALID | PTE_R | PTE_W | PTE_A | PTE_D;
}

void remove_page_table_entry(uint64_t pa) {
    // TODO
}

void create_identity_map() {
    if(!root_page_table) allocate_root_page_table();
    uint64_t physical_memory_start = 0;
    uint64_t physical_memory_end = memory_info.total_memory_base + memory_info.total_memory_size;

    for(uint64_t pa = physical_memory_start; pa < physical_memory_end; pa = pa + DEFAULT_PAGE_SIZE) {
        create_page_table_entry(pa);
    }

    enable_virtual_memory((uint64_t)root_page_table);
}
