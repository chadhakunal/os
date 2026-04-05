#ifndef TRAP_FRAME_H
#define TRAP_FRAME_H

#include "types.h"

typedef struct trap_frame {
  /* General purpose registers */
  uint64_t ra;   // x1  - return address
  uint64_t sp;   // x2  - stack pointer
  uint64_t gp;   // x3  - global pointer
  uint64_t tp;   // x4  - thread pointer
  uint64_t t0;   // x5  - temporary
  uint64_t t1;   // x6  - temporary
  uint64_t t2;   // x7  - temporary
  uint64_t s0;   // x8  - saved / frame pointer
  uint64_t s1;   // x9  - saved
  uint64_t a0;   // x10 - argument/return
  uint64_t a1;   // x11 - argument/return
  uint64_t a2;   // x12 - argument
  uint64_t a3;   // x13 - argument
  uint64_t a4;   // x14 - argument
  uint64_t a5;   // x15 - argument
  uint64_t a6;   // x16 - argument
  uint64_t a7;   // x17 - argument
  uint64_t s2;   // x18 - saved
  uint64_t s3;   // x19 - saved
  uint64_t s4;   // x20 - saved
  uint64_t s5;   // x21 - saved
  uint64_t s6;   // x22 - saved
  uint64_t s7;   // x23 - saved
  uint64_t s8;   // x24 - saved
  uint64_t s9;   // x25 - saved
  uint64_t s10;  // x26 - saved
  uint64_t s11;  // x27 - saved
  uint64_t t3;   // x28 - temporary
  uint64_t t4;   // x29 - temporary
  uint64_t t5;   // x30 - temporary
  uint64_t t6;   // x31 - temporary

  /* Supervisor CSRs */
  uint64_t sepc;    // Exception program counter
  uint64_t sstatus; // Status register
  uint64_t stval;   // Bad address or instruction
  uint64_t scause;  // Trap cause
} trap_frame_t;

#endif
