#include "kernel/drivers/plic.h"
#include "platform.h"
#include "arch/riscv64/virtual_memory_init.h"
#define DEBUG 0
#include "lib/printk/printk.h"

/*
 * RISC-V PLIC memory map (relative to plic_base):
 *
 *   +0x000000         priority registers: 4 bytes per source (source 0 unused)
 *   +0x001000         pending bits: 1 bit per source
 *   +0x002000         enable bits per (hart, context): 0x80 bytes per context
 *                     context = hart*2+1 for supervisor mode
 *   +0x200000         per-context registers:
 *     +0x000          priority threshold (4 bytes)
 *     +0x004          claim/complete register (4 bytes)
 *
 * On QEMU virt, hart 0 supervisor context = 1.
 */

#define PLIC_PRIORITY(source)     (plic_base + 0x000000 + (source) * 4)
#define PLIC_PENDING              (plic_base + 0x001000)
#define PLIC_ENABLE(context)      (plic_base + 0x002000 + (context) * 0x80)
#define PLIC_THRESHOLD(context)   (plic_base + 0x200000 + (context) * 0x1000)
#define PLIC_CLAIM(context)       (plic_base + 0x200004 + (context) * 0x1000)

/* hart 0, supervisor mode = context 1 */
#define HART0_S_CONTEXT  1

static uint64_t plic_base = 0;

void plic_init(void) {
    plic_base = (uint64_t)MMIO_PHYS_TO_VIRT(platform.plic_base);
    if (plic_base == 0) {
        printk("plic_init: PLIC base not set in platform info!\n");
        return;
    }

    debugk("plic_init: base=0x%llx\n", plic_base);

    uint32_t virtio_irq = platform.virtio_disk.irq;
    debugk("plic_init: uart_irq=%u virtio_blk_irq=%u\n", PLIC_IRQ_UART, virtio_irq);

    /* Set priority 1 (lowest non-zero) for every IRQ we care about. */
    *(volatile uint32_t *)PLIC_PRIORITY(PLIC_IRQ_UART) = 1;
    if (virtio_irq) *(volatile uint32_t *)PLIC_PRIORITY(virtio_irq) = 1;

    /* Enable both IRQs for hart 0 supervisor context.
     * The enable region is an array of 32-bit words; word[N/32] bit[N%32]
     * covers source N. */
    volatile uint32_t *en = (volatile uint32_t *)PLIC_ENABLE(HART0_S_CONTEXT);
    en[PLIC_IRQ_UART / 32] |= (1u << (PLIC_IRQ_UART % 32));
    if (virtio_irq) en[virtio_irq / 32] |= (1u << (virtio_irq % 32));

    /* Set threshold to 0 so any priority-1 interrupt is forwarded. */
    *(volatile uint32_t *)PLIC_THRESHOLD(HART0_S_CONTEXT) = 0;
}

/* Claim the highest-priority pending interrupt.
 * Returns the IRQ source ID (0 means no pending interrupt). */
uint32_t plic_claim(void) {
    return *(volatile uint32_t *)PLIC_CLAIM(HART0_S_CONTEXT);
}

/* Signal completion of an interrupt to the PLIC. */
void plic_complete(uint32_t irq) {
    *(volatile uint32_t *)PLIC_CLAIM(HART0_S_CONTEXT) = irq;
}
