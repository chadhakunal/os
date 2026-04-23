#include "cpu_idle.h"

inline void arch_wait() {
    // TODO: Check if we need WFE
    while (1) {
        asm volatile("wfi");
    }
}
