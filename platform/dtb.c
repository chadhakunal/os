#include "platform.h"
#include "platform/dtb.h"
#include "kernel/drivers/uart.h"
#include "lib/string.h"

#define DEBUG_DTB 0

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

static void uart_print_n(const char *s, int n)
{
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\0') break;
        uart_putc(c);
    }
}

void dtb_walk(void *dtb, uint32_t off_struct, uint32_t off_strings, uint32_t size_struct)
{
    uint8_t *struct_base  = (uint8_t*)dtb + off_struct;
    uint8_t *strings_base = (uint8_t*)dtb + off_strings;

    uint32_t i = 0;

    bool in_memory = false;
    bool in_virtio = false;
    bool in_uart   = false;

    uart_print("DTB WALK START\n");

    while (i < size_struct) {

        uint32_t token = fdt_u32(struct_base + i);
        i += 4;

        if (token == FDT_BEGIN_NODE) {

            char *name = (char*)(struct_base + i);

            uart_print("NODE: ");
            uart_print_n(name, 64);
            uart_print("\n");

            if (strneq_prefix(name, "memory", 6))
                in_memory = true;

            if (strneq_prefix(name, "virtio", 6))
                in_virtio = true;

            if (strneq_prefix(name, "uart", 4))
                in_uart = true;

            while (struct_base[i] != '\0')
                i++;

            i++;
            i = (i + 3) & ~3;
        }

        else if (token == FDT_END_NODE) {

            in_memory = false;
            in_virtio = false;
            in_uart   = false;

            uart_print("END NODE\n");
        }

        else if (token == FDT_PROP) {

            uint32_t len = fdt_u32(struct_base + i);
            i += 4;

            uint32_t name_off = fdt_u32(struct_base + i);
            i += 4;

            char *prop_name = (char*)(strings_base + name_off);
            uint8_t *value  = struct_base + i;

            uart_print("  PROP: ");
            uart_print(prop_name);
            uart_print("\n");

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

                        uart_print("    RAM base: ");
                        uart_print_hex(base);

                        uart_print("    RAM size: ");
                        uart_print_hex(size);
                    }
                }

                /* most MMIO devices use 32-bit address + size */
                else if (len == 8) {

                    uint64_t base = fdt_u32(value);

                    if (in_virtio) {

                        int idx = platform.virtio_count++;

                        platform.virtio[idx].base = base;

                        uart_print("    VIRTIO base: ");
                        uart_print_hex(base);
                    }

                    if (in_uart) {

                        platform.uart.base = base;

                        uart_print("    UART base: ");
                        uart_print_hex(base);
                    }
                }
            }

            /* ---------------- COMPATIBLE PROPERTY ---------------- */

            if (strneq_prefix(prop_name, "compatible", 10)) {

                uart_print("    compatible: ");
                uart_print_n((char*)value, len);
                uart_print("\n");
            }

            /* ---------------- INTERRUPTS PROPERTY ---------------- */

            if (strneq_prefix(prop_name, "interrupts", 10)) {

                if (len >= 4) {

                    uint32_t irq = fdt_u32(value);

                    if (in_virtio && platform.virtio_count > 0) {

                        int idx = platform.virtio_count - 1;
                        platform.virtio[idx].irq = irq;

                        uart_print("    VIRTIO IRQ: ");
                        uart_print_hex(irq);
                    }

                    if (in_uart) {

                        // platform.uart.irq = irq;

                        uart_print("    UART IRQ: ");
                        uart_print_hex(irq);
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
            uart_print("DTB END\n");
            break;
        }

        else {
            uart_print("UNKNOWN TOKEN\n");
            break;
        }
    }
}

uint32_t platform_init(void* dtb) {
    /* Do nothing for now - just return success so we can reach kmain */
    return 0;
}
