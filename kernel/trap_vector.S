.section .text
.global trap_vector
.align 4

trap_vector:
    # Allocate space for trap_frame on stack (36 * 8 = 288 bytes)
    addi sp, sp, -288

    # Save all general purpose registers
    sd ra, 0(sp)
    sd sp, 8(sp)     # Save original sp (will fix later)
    sd gp, 16(sp)
    sd tp, 24(sp)
    sd t0, 32(sp)
    sd t1, 40(sp)
    sd t2, 48(sp)
    sd s0, 56(sp)
    sd s1, 64(sp)
    sd a0, 72(sp)
    sd a1, 80(sp)
    sd a2, 88(sp)
    sd a3, 96(sp)
    sd a4, 104(sp)
    sd a5, 112(sp)
    sd a6, 120(sp)
    sd a7, 128(sp)
    sd s2, 136(sp)
    sd s3, 144(sp)
    sd s4, 152(sp)
    sd s5, 160(sp)
    sd s6, 168(sp)
    sd s7, 176(sp)
    sd s8, 184(sp)
    sd s9, 192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)

    # Save CSRs
    csrr t0, sepc
    sd t0, 248(sp)
    csrr t0, sstatus
    sd t0, 256(sp)
    csrr t0, stval
    sd t0, 264(sp)
    csrr t0, scause
    sd t0, 272(sp)

    # Fix saved sp (original was sp + 288)
    addi t0, sp, 288
    sd t0, 8(sp)

    # Pass trap_frame pointer as first argument (a0 = sp)
    mv a0, sp

    # Call C trap handler (NEVER RETURNS - it calls trap_return or panics)
    call trap_handler

    # Should never reach here
    j .

# trap_return(trap_frame_t *tf)
# Restores CPU state from trap frame and returns to interrupted execution
.global trap_return
trap_return:
    # a0 = trap_frame pointer
    # DEBUG: Write a marker value to show we entered trap_return
    li t0, 0xDEADBEEF
    mv sp, a0

    # Restore CSRs (sepc and sstatus)
    ld t0, 248(sp)
    csrw sepc, t0
    ld t0, 256(sp)
    csrw sstatus, t0

    # Restore all general purpose registers
    ld ra, 0(sp)
    # Don't restore sp yet, we need it to access the frame
    ld gp, 16(sp)
    ld tp, 24(sp)
    ld t0, 32(sp)
    ld t1, 40(sp)
    ld t2, 48(sp)
    ld s0, 56(sp)
    ld s1, 64(sp)
    ld a0, 72(sp)
    ld a1, 80(sp)
    ld a2, 88(sp)
    ld a3, 96(sp)
    ld a4, 104(sp)
    ld a5, 112(sp)
    ld a6, 120(sp)
    ld a7, 128(sp)
    ld s2, 136(sp)
    ld s3, 144(sp)
    ld s4, 152(sp)
    ld s5, 160(sp)
    ld s6, 168(sp)
    ld s7, 176(sp)
    ld s8, 184(sp)
    ld s9, 192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3, 216(sp)
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)

    # Finally restore sp and return to user mode
    ld sp, 8(sp)
    sret
