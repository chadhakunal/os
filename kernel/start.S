.section .text.init
.global _start
_start:
  la sp, _stack_top
  call boot
  j .
