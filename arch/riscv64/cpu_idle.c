#include "cpu_idle.h"

inline void arch_wait() {
    while (1) {
        asm volatile("wfi");
    }
}
