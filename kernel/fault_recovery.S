.section .text
.global kernel_fault_recovery_setup
.global kernel_fault_recovery_jump

# int kernel_fault_recovery_setup(struct kernel_fault_recovery_t *r)
# Saves sp, ra, s0-s11 into *r and marks it active.
# Returns 0 on first call; when kernel_fault_recovery_jump is called,
# execution resumes here returning 1.
kernel_fault_recovery_setup:
  sd sp,   8(a0)
  sd ra,  16(a0)
  sd s0,  24(a0)
  sd s1,  32(a0)
  sd s2,  40(a0)
  sd s3,  48(a0)
  sd s4,  56(a0)
  sd s5,  64(a0)
  sd s6,  72(a0)
  sd s7,  80(a0)
  sd s8,  88(a0)
  sd s9,  96(a0)
  sd s10,104(a0)
  sd s11,112(a0)
  li t0, 1
  sd t0,   0(a0)   # active = 1
  li a0, 0         # return 0
  ret

# void kernel_fault_recovery_jump(struct kernel_fault_recovery_t *r)
# Restores sp, ra, s0-s11 and returns 1 to the kernel_fault_recovery_setup call site.
kernel_fault_recovery_jump:
  ld sp,   8(a0)
  ld ra,  16(a0)
  ld s0,  24(a0)
  ld s1,  32(a0)
  ld s2,  40(a0)
  ld s3,  48(a0)
  ld s4,  56(a0)
  ld s5,  64(a0)
  ld s6,  72(a0)
  ld s7,  80(a0)
  ld s8,  88(a0)
  ld s9,  96(a0)
  ld s10,104(a0)
  ld s11,112(a0)
  li a0, 1         # return 1 to setup call site
  ret
