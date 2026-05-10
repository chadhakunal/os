#ifndef SIGNAL_JUMP_POINT_H
#define SIGNAL_JUMP_POINT_H

#include "types.h"

// Fixed user-space address for signal jump point page
// Located above the user stack (stack top is at 0x80000000)
#define SIGNAL_JUMP_POINT_ADDR 0x80001000ULL

void init_signal_jump_point(void);

void *get_signal_jump_point_page(void);

#endif
