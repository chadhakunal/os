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

struct pci_device {
    uint64_t base;
    uint64_t size;

    uint64_t mmio64_pref_base;
    uint64_t mmio64_pref_size;

    uint32_t mmio32_base;
    uint32_t mmio32_size;
};

struct pci_virtio_blk_device {
    uint64_t common_cfg_mmio_reg;
    uint64_t notify_cfg_mmio_reg;
    uint32_t notify_off_multiplier;
    uint64_t isr_cfg_mmio_reg;
    uint64_t device_cfg_mmio_reg;
    uint32_t irq;
};

struct platform_info {
    struct cpu_core core;
    struct memory_region ram;
    struct device uart;
    struct device rtc;
    struct pci_device pci;
    struct pci_virtio_blk_device virtio_disk;

    struct virtio_dev virtio[8];
    int virtio_count;

    uint64_t plic_base;
};


extern volatile struct platform_info platform;

uint32_t platform_init(void* boot_data);

#endif
