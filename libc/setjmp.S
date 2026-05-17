/* RISC-V lp64 setjmp/longjmp (integer ABI only; no FP callee-saves). */
.section .text
.global setjmp
.global __setjmp
.global longjmp
.global siglongjmp
.type setjmp, %function
.type __setjmp, %function
.type longjmp, %function
.type siglongjmp, %function

/* int setjmp(jmp_buf env); env in a0 */
setjmp:
__setjmp:
	sd s0,    0(a0)
	sd s1,    8(a0)
	sd s2,   16(a0)
	sd s3,   24(a0)
	sd s4,   32(a0)
	sd s5,   40(a0)
	sd s6,   48(a0)
	sd s7,   56(a0)
	sd s8,   64(a0)
	sd s9,   72(a0)
	sd s10,  80(a0)
	sd s11,  88(a0)
	sd sp,   96(a0)
	sd ra,  104(a0)
	li a0, 0
	ret

/* void longjmp(jmp_buf env, int val); env in a0, val in a1 */
longjmp:
	mv t0, a0
	mv t1, a1
	ld s0,    0(t0)
	ld s1,    8(t0)
	ld s2,   16(t0)
	ld s3,   24(t0)
	ld s4,   32(t0)
	ld s5,   40(t0)
	ld s6,   48(t0)
	ld s7,   56(t0)
	ld s8,   64(t0)
	ld s9,   72(t0)
	ld s10,  80(t0)
	ld s11,  88(t0)
	ld sp,   96(t0)
	ld ra,  104(t0)
	mv a1, t1
	seqz a0, a1
	add  a0, a0, a1
	ret

/* void siglongjmp(jmp_buf env, int val) */
siglongjmp:
	ld a5, 112(a0)
	mv t0, a0
	mv t1, a1
	beqz a5, 1f
	addi a1, t0, 120
	li a2, 0
	li a0, 2
	call sigprocmask
1:
	mv a0, t0
	mv a1, t1
	tail longjmp
