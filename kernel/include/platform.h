#ifndef PLATFORM_H
#define PLATFORM_H

#include "types.h"

extern char _end[];
extern char _kernel_start[];

struct cpu_core {
    const char* name;
};

struct virtio_dev {
    uint64_t base;
    uint32_t irq;
};

struct memory_region {
    uint64_t base;
    uint64_t size;
};

struct device {
    uint64_t base;
    uint64_t size;
};

struct platform_info {
    struct cpu_core core;
    struct memory_region ram;
    struct device uart;

    struct virtio_dev virtio[8];
    int virtio_count;
};


extern volatile struct platform_info platform;

uint32_t platform_init(void* boot_data);

#endif
