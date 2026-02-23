#ifndef PLATFORM_H
#define PLATFORM_H

#include "types.h"

struct cpu_core {
    const char* name;
};

struct memory_region {
    uint64_t base;
    uint64_t size;    
};

struct device {
    const char* name;
    uint64_t base;
    uint64_t size;
};

struct platform_info {
    struct cpu_core core;
    struct memory_region ram;
    struct device uart;
};


extern struct platform_info platform;

uint32_t platform_init(void* boot_data);

#endif
