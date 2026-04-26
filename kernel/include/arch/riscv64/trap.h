#ifndef TRAP_H
#define TRAP_H

#include "types.h"

/* sstatus register bits */
#define SSTATUS_SIE   (1UL << 1)  /* Supervisor Interrupt Enable */
#define SSTATUS_SPIE  (1UL << 5)  /* Supervisor Previous Interrupt Enable */
#define SSTATUS_UBE   (1UL << 6)  /* User Big-Endian */
#define SSTATUS_SPP   (1UL << 8)  /* Supervisor Previous Privilege (0=User, 1=Supervisor) */
#define SSTATUS_FS    (3UL << 13) /* Floating-point Status */
#define SSTATUS_XS    (3UL << 15) /* Extension Status */
#define SSTATUS_SUM   (1UL << 18) /* Permit Supervisor User Memory access */
#define SSTATUS_MXR   (1UL << 19) /* Make eXecutable Readable */
#define SSTATUS_UXL   (3UL << 32) /* User XLEN (10=64-bit, 01=32-bit) */
#define SSTATUS_SD    (1UL << 63) /* State Dirty (FS/XS) */

/* sstatus UXL field values */
#define SSTATUS_UXL_32  (1UL << 32)
#define SSTATUS_UXL_64  (2UL << 32)

struct trap_frame {
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
  uint64_t padding; // Padding to match assembly's 288 bytes (36 * 8)
};

void init_trap_handler(void);

/* Enable global interrupts (sstatus.SIE and sie register) */
void enable_interrupts(void);

/* Disable global interrupts (sstatus.SIE) */
void disable_interrupts(void);

/* NEVER RETURNS - either calls trap_return() or panic() */
void trap_handler(void);

/* Restore CPU state from trap frame and return to interrupted execution
 * NEVER RETURNS - does sret */
void trap_return(struct trap_frame *tf) __attribute__((noreturn));

#endif
