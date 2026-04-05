#ifndef TRAP_H
#define TRAP_H

#include "trap_frame.h"

void init_trap_handler(void);

/* Restore CPU state from trap frame and return to interrupted execution
 * NEVER RETURNS - does sret */
void trap_return(trap_frame_t *tf) __attribute__((noreturn));

#endif
