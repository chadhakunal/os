.section .text
.global _start

.extern __bss_start
.extern __bss_end
.extern boot_entry

_start:
  // On entry: a0 = hart ID, a1 = DTB pointer (set by OpenSBI)
  // Save DTB pointer before we clobber registers
  mv      s0, a1

  la      t0, stack_top
  mv      sp, t0

  la      t0, __bss_start
  la      t1, __bss_end

clear_bss:
  bgeu    t0, t1, bss_done
  sd      zero, 0(t0)
  addi    t0, t0, 8
  j       clear_bss

bss_done:
  mv      a0, s0          // pass DTB pointer as first argument
  call    boot_entry

hang:
  wfi
  j       hang

.section .bss
.align 12                   // 4096-byte alignment
stack:
  .skip   16384           // 16 KB stack
stack_top:
