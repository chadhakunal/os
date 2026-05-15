#ifndef PAGE_FAULT_H
#define PAGE_FAULT_H

#include "types.h"
#include "arch/riscv64/trap.h"

int handle_page_fault(uint64_t fault_addr, uint64_t scause, struct trap_frame *tf);

#endif
