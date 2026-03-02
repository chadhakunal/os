#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

struct memory_info {
    uint64_t kernel_start;
    uint64_t kernel_end;
    uint64_t kernel_size;
    
    uint64_t total_memory_base;
    uint64_t total_memory_size;
    
    uint64_t heap_start;
    uint64_t heap_size;
    uint64_t heap_used;
};

extern struct memory_info kernel_memory;

void memory_init();

void print_memory();

#endif
