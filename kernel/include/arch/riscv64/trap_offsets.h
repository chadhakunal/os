#ifndef TRAP_OFFSETS_H
#define TRAP_OFFSETS_H

#include <stddef.h>
#include "arch/riscv64/trap.h"

/* Offsets into struct trap_frame for assembly code */
#define TF_RA       offsetof(struct trap_frame, ra)       // 0
#define TF_SP       offsetof(struct trap_frame, sp)       // 8
#define TF_GP       offsetof(struct trap_frame, gp)       // 16
#define TF_TP       offsetof(struct trap_frame, tp)       // 24
#define TF_T0       offsetof(struct trap_frame, t0)       // 32
#define TF_T1       offsetof(struct trap_frame, t1)       // 40
#define TF_T2       offsetof(struct trap_frame, t2)       // 48
#define TF_S0       offsetof(struct trap_frame, s0)       // 56
#define TF_S1       offsetof(struct trap_frame, s1)       // 64
#define TF_A0       offsetof(struct trap_frame, a0)       // 72
#define TF_A1       offsetof(struct trap_frame, a1)       // 80
#define TF_A2       offsetof(struct trap_frame, a2)       // 88
#define TF_A3       offsetof(struct trap_frame, a3)       // 96
#define TF_A4       offsetof(struct trap_frame, a4)       // 104
#define TF_A5       offsetof(struct trap_frame, a5)       // 112
#define TF_A6       offsetof(struct trap_frame, a6)       // 120
#define TF_A7       offsetof(struct trap_frame, a7)       // 128
#define TF_S2       offsetof(struct trap_frame, s2)       // 136
#define TF_S3       offsetof(struct trap_frame, s3)       // 144
#define TF_S4       offsetof(struct trap_frame, s4)       // 152
#define TF_S5       offsetof(struct trap_frame, s5)       // 160
#define TF_S6       offsetof(struct trap_frame, s6)       // 168
#define TF_S7       offsetof(struct trap_frame, s7)       // 176
#define TF_S8       offsetof(struct trap_frame, s8)       // 184
#define TF_S9       offsetof(struct trap_frame, s9)       // 192
#define TF_S10      offsetof(struct trap_frame, s10)      // 200
#define TF_S11      offsetof(struct trap_frame, s11)      // 208
#define TF_T3       offsetof(struct trap_frame, t3)       // 216
#define TF_T4       offsetof(struct trap_frame, t4)       // 224
#define TF_T5       offsetof(struct trap_frame, t5)       // 232
#define TF_T6       offsetof(struct trap_frame, t6)       // 240
#define TF_SEPC     offsetof(struct trap_frame, sepc)     // 248
#define TF_SSTATUS  offsetof(struct trap_frame, sstatus)  // 256
#define TF_STVAL    offsetof(struct trap_frame, stval)    // 264
#define TF_SCAUSE   offsetof(struct trap_frame, scause)   // 272

/* Size of trap frame (for stack allocation) */
#define TF_SIZE     sizeof(struct trap_frame)             // 288 bytes

#endif
