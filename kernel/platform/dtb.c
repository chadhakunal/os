#include "platform.h"
#include "platform/dtb.h"
#include "kernel/drivers/uart.h"
#include "lib/string.h"

#define DEBUG 0
#include "lib/printk/printk.h"

volatile struct platform_info platform = {0};

static inline uint32_t bswap32(uint32_t x)
{
    return ((x & 0xff000000) >> 24) |
           ((x & 0x00ff0000) >> 8)  |
           ((x & 0x0000ff00) << 8)  |
           ((x & 0x000000ff) << 24);
}

static inline uint32_t fdt_u32(const void *p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return bswap32(v);
}

void dtb_walk(void *dtb, uint32_t off_struct, uint32_t off_strings, uint32_t size_struct)
{
    uint8_t *struct_base  = (uint8_t*)dtb + off_struct;
    uint8_t *strings_base = (uint8_t*)dtb + off_strings;

    uint32_t i = 0;

    bool in_memory = false;
    bool in_virtio = false;
    bool in_uart   = false;
    bool in_rtc    = false;
    bool in_pci    = false;

    debugk("DTB WALK START\n");

    while (i < size_struct) {

        uint32_t token = fdt_u32(struct_base + i);
        i += 4;

        if (token == FDT_BEGIN_NODE) {

            char *name = (char*)(struct_base + i);

            debugk("NODE: %s\n", name);

            if (strneq_prefix(name, "memory", 6))
                in_memory = true;

            if (strneq_prefix(name, "virtio", 6))
                in_virtio = true;

            if (strneq_prefix(name, "uart", 4))
                in_uart = true;

            if (strneq_prefix(name, "rtc", 3))
                in_rtc = true;

            if (strneq_prefix(name, "pci", 3))
                in_pci = true;

            while (struct_base[i] != '\0')
                i++;

            i++;
            i = (i + 3) & ~3;
        }

        else if (token == FDT_END_NODE) {

            in_memory = false;
            in_virtio = false;
            in_uart   = false;
            in_rtc    = false;
            in_pci    = false;

            debugk("END NODE\n");
        }

        else if (token == FDT_PROP) {

            uint32_t len = fdt_u32(struct_base + i);
            i += 4;

            uint32_t name_off = fdt_u32(struct_base + i);
            i += 4;

            char *prop_name = (char*)(strings_base + name_off);
            uint8_t *value  = struct_base + i;

            debugk("  PROP: %s\n", prop_name);

            /* ---------------- REG PROPERTY ---------------- */

            if (strneq_prefix(prop_name, "reg", 3)) {

                /* memory node uses 64-bit address + size */
                if (len == 16) {

                    uint64_t base =
                        ((uint64_t)fdt_u32(value) << 32) |
                        fdt_u32(value + 4);

                    uint64_t size =
                        ((uint64_t)fdt_u32(value + 8) << 32) |
                        fdt_u32(value + 12);

                    if (in_memory) {

                        platform.ram.base = base;
                        platform.ram.size = size;

                        debugk("    RAM base: 0x%llx\n", base);
                        debugk("    RAM size: 0x%llx\n", size);
                    }

                    if (in_pci) {
                        platform.pci.base = base;
                        platform.pci.size = size;

                        debugk("    PCI base: 0x%llx\n", base);
                        debugk("    PCI size: 0x%llx\n", size);
                    }
                }

                /* most MMIO devices use 32-bit address + size */
                else if (len == 8) {

                    uint64_t base = fdt_u32(value);

                    if (in_virtio) {

                        int idx = platform.virtio_count++;

                        platform.virtio[idx].base = base;

                        debugk("    VIRTIO base: 0x%llx\n", base);
                    }

                    if (in_uart) {

                        platform.uart.base = base;

                        debugk("    UART base: 0x%llx\n", base);
                    }

                    if (in_rtc) {

                        platform.rtc.base = base;

                        debugk("    RTC base: 0x%llx\n", base);
                    }
                }
            }

            /* ---------------- COMPATIBLE PROPERTY ---------------- */

            if (strneq_prefix(prop_name, "compatible", 10)) {

                debugk("    compatible: %s\n", (char*)value);
            }

            /* ---------------- INTERRUPTS PROPERTY ---------------- */

            if (strneq_prefix(prop_name, "interrupts", 10)) {

                if (len >= 4) {

                    uint32_t irq = fdt_u32(value);

                    if (in_virtio && platform.virtio_count > 0) {

                        int idx = platform.virtio_count - 1;
                        platform.virtio[idx].irq = irq;

                        debugk("    VIRTIO IRQ: 0x%x\n", irq);
                    }

                    if (in_uart) {

                        // platform.uart.irq = irq;

                        debugk("    UART IRQ: 0x%x\n", irq);
                    }
                }
            }

            if (strneq_prefix(prop_name, "ranges", 6)) {
                if(in_pci) {
                    for (uint32_t off = 0; off < len; off += 28) {
                        uint32_t flags = fdt_u32(value + off + 0);

                        uint64_t pci_addr =
                            ((uint64_t)fdt_u32(value + off + 4) << 32) |
                            fdt_u32(value + off + 8);

                        uint64_t cpu_addr =
                            ((uint64_t)fdt_u32(value + off + 12) << 32) |
                            fdt_u32(value + off + 16);

                        uint64_t size =
                            ((uint64_t)fdt_u32(value + off + 20) << 32) |
                            fdt_u32(value + off + 24);

                        debugk("    PCI range flags=0x%x pci=0x%llx cpu=0x%llx size=0x%llx\n", flags, pci_addr, cpu_addr, size);

                        if (flags == 0x02000000) {
                            platform.pci.mmio32_base = cpu_addr;
                            platform.pci.mmio32_size = size;

                            debugk("    PCI MMIO32 base=0x%llx size=0x%llx\n", cpu_addr, size);
                        }

                        if (flags == 0x03000000) {
                            platform.pci.mmio64_pref_base = cpu_addr;
                            platform.pci.mmio64_pref_size = size;

                            debugk("    PCI MMIO64 prefetch base=%llx size=%llx\n", cpu_addr, size);
                        }
                    }
                }
            }

            /* advance to next token */
            i += len;
            i = (i + 3) & ~3;
        }

        else if (token == FDT_NOP) {
            /* ignore */
        }

        else if (token == FDT_END) {
            debugk("DTB END\n");
            break;
        }

        else {
            debugk("UNKNOWN TOKEN\n");
            break;
        }
    }
}

uint32_t enumerate_pci_device() {
    uint32_t num_buses = platform.pci.size >> 20;

    for (uint32_t bus = 0; bus < num_buses; bus++) {
        for (uint32_t dev = 0; dev < 32; dev++) {
            for (uint32_t func = 0; func < 8; func++) {
                uint64_t pci_config = platform.pci.base + (bus  << 20) + (dev  << 15) + (func << 12);
                uint16_t vendor = *(volatile uint16_t *)(pci_config + 0x00);

                if (vendor == 0xffff)
                    continue; 

                uint16_t device = *(volatile uint16_t *)(pci_config + 0x02);
                if (vendor == 0x1af4 && device == 0x1042) {
                    uint8_t header_type = *(volatile uint8_t *)(pci_config + 0x0e);
                    header_type &= 0x7f;
                    
                    uint16_t cmd = *(volatile uint16_t *)(pci_config + 0x04);
                    uint8_t cap_ptr = *(volatile uint8_t *)(pci_config + 0x34);
                    debugk("    Found virtio-blk\n");
                    debugk("    vendor ID: %x\n", vendor);
                    debugk("    device ID: %x\n", device);
                    debugk("    command: %x\n", cmd);
                    debugk("    status: %x\n", *(volatile uint16_t *)(pci_config + 0x06));
                    debugk("    header type: %x\n", header_type);
                    
                    uint64_t bar_base_addr[8];

                    if(header_type == 0) {
                        for (int i = 0; i < 6; i++) {
                            uint32_t off = 0x10 + i * 4;
                            uint32_t bar = *(volatile uint32_t *)(pci_config + off);

                            if (bar == 0) {
                                debugk("    BAR%d unused\n", i);
                                continue;
                            }

                            if (bar & 0x1) {
                                // I/O BAR
                                uint32_t io_base = bar & ~0x3;
                                (void)io_base;
                                debugk("    BAR%d I/O raw=%x base=%x\n", i, bar, io_base);
                            } else {
                                // Memory/MMIO BAR
                                uint32_t type = (bar >> 1) & 0x3;
                                uint32_t prefetch = (bar >> 3) & 0x1;

                                if (type == 0x0) {
                                    // 32-bit MMIO BAR
                                    uint32_t base = bar & ~0xf;
                                    (void)base;
                                    (void)prefetch;
                                    debugk("    BAR%d MMIO32 raw=%x base=%x prefetch=%u\n",
                                        i, bar, base, prefetch);
                                } else if (type == 0x2) {
                                    // 64-bit MMIO BAR, consumes BAR i and BAR i+1
                                    uint32_t bar_hi = *(volatile uint32_t *)(pci_config + off + 4);

                                    uint64_t base =
                                        ((uint64_t)bar_hi << 32) |
                                        (uint64_t)(bar & ~0xf);
                                    (void)base;
                                    (void)prefetch;

                                    debugk("    BAR%d/BAR%d MMIO64 raw_lo=%x raw_hi=%x base=%llx prefetch=%u\n",
                                        i, i + 1, bar, bar_hi, base, prefetch);

                                    uint32_t old_lo = *(volatile uint32_t *)(pci_config + 0x20); // BAR4
                                    uint32_t old_hi = *(volatile uint32_t *)(pci_config + 0x24); // BAR5

                                    *(volatile uint32_t *)(pci_config + 0x20) = 0xffffffff;
                                    *(volatile uint32_t *)(pci_config + 0x24) = 0xffffffff;

                                    uint32_t mask_lo = *(volatile uint32_t *)(pci_config + 0x20);
                                    uint32_t mask_hi = *(volatile uint32_t *)(pci_config + 0x24);

                                    *(volatile uint32_t *)(pci_config + 0x20) = old_lo;
                                    *(volatile uint32_t *)(pci_config + 0x24) = old_hi;

                                    uint64_t mask = ((uint64_t)mask_hi << 32) | (mask_lo & ~0xfULL);
                                    uint64_t size = (~mask) + 1;

                                    uint64_t bar_base = platform.pci.mmio64_pref_base;
                                    debugk("    BAR BASE: %llx\n", bar_base);
                                    if(size > platform.pci.mmio64_pref_size) {
                                        debugk("[PANIC] Not enough ranges for PCI MMIO!");
                                        return -1;
                                    }

                                    *(volatile uint32_t *)(pci_config + 0x20) = (uint32_t)(bar_base & 0xfffffff0) | (old_lo & 0xf);
                                    *(volatile uint32_t *)(pci_config + 0x24) = (uint32_t)(bar_base >> 32);

                                    bar_base_addr[i] = bar_base;

                                    i++; // skip next BAR slot
                                } else {
                                    debugk("    BAR%d unknown MMIO type raw=%x type=%u\n", i, bar, type);
                                }
                            }
                        }
                    }

                    debugk("    cap ptr: %x\n", cap_ptr);
                    uint8_t cap = cap_ptr;

                    while (cap != 0) {
                        uint8_t cap_id = *(volatile uint8_t *)(pci_config + cap + 0x00);
                        uint8_t next   = *(volatile uint8_t *)(pci_config + cap + 0x01);

                        debugk("    cap @%x id=%x next=%x\n", cap, cap_id, next);

                        if (cap_id == 0x09) {
                            uint8_t cap_len  = *(volatile uint8_t  *)(pci_config + cap + 0x02);
                            uint8_t cfg_type = *(volatile uint8_t  *)(pci_config + cap + 0x03);
                            uint8_t bar      = *(volatile uint8_t  *)(pci_config + cap + 0x04);

                            uint32_t offset  = *(volatile uint32_t *)(pci_config + cap + 0x08);
                            uint32_t length  = *(volatile uint32_t *)(pci_config + cap + 0x0c);
                            (void)length;

                            debugk("        virtio cap: len=%u cfg_type=%u bar=%u offset=%x length=%x\n", cap_len, cfg_type, bar, offset, length);

                            if (cfg_type == 1) platform.virtio_disk.common_cfg_mmio_reg = bar_base_addr[bar] + offset;
                            if (cfg_type == 2) {
                                platform.virtio_disk.notify_cfg_mmio_reg = bar_base_addr[bar] + offset;
                                if (cap_len >= 20) {
                                    platform.virtio_disk.notify_off_multiplier = *(volatile uint32_t *)(pci_config + cap + 0x10);
                                } else {
                                    printk("virtio notify cap too short\n");
                                    return -1;
                                }
                            }
                            if (cfg_type == 3) platform.virtio_disk.isr_cfg_mmio_reg    = bar_base_addr[bar] + offset;
                            if (cfg_type == 4) platform.virtio_disk.device_cfg_mmio_reg = bar_base_addr[bar] + offset;
                        }

                        cap = next & ~0x3;
                    }

                    cmd |= (1 << 1); // memory space enable
                    cmd |= (1 << 2); // bus master enable

                    *(volatile uint16_t *)(pci_config + 0x04) = cmd;
                }
            }
        }
    }

    return 0;
}

uint32_t platform_init(void* dtb) {
    if (!dtb) {
        return 0;
    }
    
    struct fdt_header* hdr = (struct fdt_header*)dtb;

    uint32_t magic = bswap32(hdr->magic);
    uint32_t off_dt_struct = bswap32(hdr->off_dt_struct);
    uint32_t size_dt_struct = bswap32(hdr->size_dt_struct);
    uint32_t off_dt_strings = bswap32(hdr->off_dt_strings);

    if(magic != EXPECTED_MAGIC) {
        return -1;
    }

    dtb_walk(dtb, off_dt_struct, off_dt_strings, size_dt_struct);
    
    if(platform.pci.base != 0) {
        debugk("Enumerating PCI Devices: \n");
        enumerate_pci_device();
    }

    debugk("VIRTIO DISK: \n");
    debugk("    common_cfg_mmio_reg: %llx\n", platform.virtio_disk.common_cfg_mmio_reg);
    debugk("    notify_cfg_mmio_reg: %llx\n", platform.virtio_disk.notify_cfg_mmio_reg);
    debugk("    isr_cfg_mmio_reg: %llx\n", platform.virtio_disk.isr_cfg_mmio_reg);
    debugk("    device_cfg_mmio_reg: %llx\n", platform.virtio_disk.device_cfg_mmio_reg);

    return 0;
}
