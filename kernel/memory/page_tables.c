#include "types.h"
#include "platform.h"
#include "kernel/panic.h"

#include "kernel/memory/page_allocator.h"
#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_tables.h"

#include "virtual_memory.h"
#include "lib/string.h"
#include "lib/printk/printk.h"

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

void create_page_table_entry(uint64_t va) {
    uint64_t pt1_idx = PT1_OFFSET(va);  // VPN[2]
    uint64_t pt2_idx = PT2_OFFSET(va);  // VPN[1]
    uint64_t pt3_idx = PT3_OFFSET(va);  // VPN[0]

    page_table_t* pt2;
    page_table_t* pt3;
    
    // Root table (pt1 == root_page_table) is indexed by VPN[2]
    if(root_page_table->page_table_entries[pt1_idx] == 0) {
        pt2 = allocate_page_table();
        if(!pt2) panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
        root_page_table->page_table_entries[pt1_idx] = PTE_ADDR(pt2) | PTE_VALID | PTE_TABLE;
    } else {
        pt2 = (page_table_t*)PTE_DECODE(root_page_table->page_table_entries[pt1_idx]);
    }

    if(pt2->page_table_entries[pt2_idx] == 0) {
        pt3 = allocate_page_table();
        if(!pt3) panic("FAILED TO ALLOCATE NEW PAGE TABLE!");
        pt2->page_table_entries[pt2_idx] = PTE_ADDR(pt3) | PTE_VALID | PTE_TABLE;
    } else {
        pt3 = (page_table_t*)PTE_DECODE(pt2->page_table_entries[pt2_idx]);
    }

    pt3->page_table_entries[pt3_idx] = PTE_ADDR(va) | PTE_VALID | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D;
}

void remove_page_table_entry(uint64_t pa) {
    // TODO
}

void create_identity_map() {
    if(!root_page_table) allocate_root_page_table();
    printk("Root page table located at %lx\n", root_page_table);
    // For testing, only map the kernel region + 4MB of RAM
    uint64_t physical_memory_start = memory_info.total_memory_base;
    uint64_t physical_memory_end = physical_memory_start + memory_info.total_memory_size;
    
    printk("Creating minimal identity map from 0x%lx to 0x%lx\n", physical_memory_start, physical_memory_end);
    uint64_t count = 0;
    for(uint64_t pa = physical_memory_start; pa < physical_memory_end; pa = pa + DEFAULT_PAGE_SIZE) {
        create_page_table_entry(pa);
        //printk("Mapped Physical Memory %lx\n", pa);
        count++;
    }
    
    printk("Minimal identity map created: %lu pages mapped\n", count);
    
    printk("Root PT at 0x%lx, enabling virtual memory...\n", (uint64_t)root_page_table);
    
    enable_virtual_memory((uint64_t)root_page_table);
}
