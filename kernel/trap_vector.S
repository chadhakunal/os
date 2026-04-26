#include "arch/riscv64/trap_offsets.h"

.section .text
.global trap_vector
.align 4

trap_vector:
  # When coming from user mode, sscratch contains the kernel stack top
  # When coming from kernel mode, sscratch is 0 (for now, we don't handle this)
  # Swap sp with sscratch to switch to kernel stack
  csrrw sp, sscratch, sp
  # Now: sp = kernel stack top, sscratch = user stack pointer

  # Allocate space for trap_frame on kernel stack
  addi sp, sp, -TF_SIZE

  # Save all general purpose registers
  sd ra, TF_RA(sp)
  # Save user sp (currently in sscratch) to trap frame
  csrr t0, sscratch
  sd t0, TF_SP(sp)
  sd gp, TF_GP(sp)
  sd tp, TF_TP(sp)
  sd t0, TF_T0(sp)
  sd t1, TF_T1(sp)
  sd t2, TF_T2(sp)
  sd s0, TF_S0(sp)
  sd s1, TF_S1(sp)
  sd a0, TF_A0(sp)
  sd a1, TF_A1(sp)
  sd a2, TF_A2(sp)
  sd a3, TF_A3(sp)
  sd a4, TF_A4(sp)
  sd a5, TF_A5(sp)
  sd a6, TF_A6(sp)
  sd a7, TF_A7(sp)
  sd s2, TF_S2(sp)
  sd s3, TF_S3(sp)
  sd s4, TF_S4(sp)
  sd s5, TF_S5(sp)
  sd s6, TF_S6(sp)
  sd s7, TF_S7(sp)
  sd s8, TF_S8(sp)
  sd s9, TF_S9(sp)
  sd s10, TF_S10(sp)
  sd s11, TF_S11(sp)
  sd t3, TF_T3(sp)
  sd t4, TF_T4(sp)
  sd t5, TF_T5(sp)
  sd t6, TF_T6(sp)

  # Save CSRs
  csrr t0, sepc
  sd t0, TF_SEPC(sp)
  csrr t0, sstatus
  sd t0, TF_SSTATUS(sp)
  csrr t0, stval
  sd t0, TF_STVAL(sp)
  csrr t0, scause
  sd t0, TF_SCAUSE(sp)

  # Pass trap_frame pointer as first argument (a0 = sp)
  mv a0, sp

  # Call C trap handler (NEVER RETURNS - it calls trap_return or panics)
  call trap_handler

  # Should never reach here
  j .

# trap_return(trap_frame_t *tf)
# Restores CPU state from trap frame and returns to interrupted execution
# NOTE: Caller must set sscratch to kernel stack top before calling this
.global trap_return
trap_return:
  # a0 = trap_frame pointer
  mv sp, a0

  # Restore CSRs (sepc and sstatus)
  ld t0, TF_SEPC(sp)
  csrw sepc, t0
  ld t0, TF_SSTATUS(sp)
  csrw sstatus, t0

  # Restore all general purpose registers
  ld ra, TF_RA(sp)
  # Don't restore sp yet, we need it to access the frame
  ld gp, TF_GP(sp)
  ld tp, TF_TP(sp)
  ld t0, TF_T0(sp)
  ld t1, TF_T1(sp)
  ld t2, TF_T2(sp)
  ld s0, TF_S0(sp)
  ld s1, TF_S1(sp)
  ld a0, TF_A0(sp)
  ld a1, TF_A1(sp)
  ld a2, TF_A2(sp)
  ld a3, TF_A3(sp)
  ld a4, TF_A4(sp)
  ld a5, TF_A5(sp)
  ld a6, TF_A6(sp)
  ld a7, TF_A7(sp)
  ld s2, TF_S2(sp)
  ld s3, TF_S3(sp)
  ld s4, TF_S4(sp)
  ld s5, TF_S5(sp)
  ld s6, TF_S6(sp)
  ld s7, TF_S7(sp)
  ld s8, TF_S8(sp)
  ld s9, TF_S9(sp)
  ld s10, TF_S10(sp)
  ld s11, TF_S11(sp)
  ld t3, TF_T3(sp)
  ld t4, TF_T4(sp)
  ld t5, TF_T5(sp)
  ld t6, TF_T6(sp)

  # Finally restore user sp and return to user mode
  ld sp, TF_SP(sp)
  sret
