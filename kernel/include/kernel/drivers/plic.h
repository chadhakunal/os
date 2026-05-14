#ifndef KERNEL_DRIVERS_PLIC_H
#define KERNEL_DRIVERS_PLIC_H

#include "types.h"

/* UART0 is always IRQ 10 on QEMU virt.
 * virtio-blk IRQ is read from PCI config at boot; use platform.virtio_disk.irq. */
#define PLIC_IRQ_UART  10

void     plic_init(void);
uint32_t plic_claim(void);
void     plic_complete(uint32_t irq);

#endif
