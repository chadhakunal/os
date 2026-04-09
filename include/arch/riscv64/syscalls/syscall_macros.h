#ifndef SYSCALL_MACROS_H
#define SYSCALL_MACROS_H

#include "types.h"
// tx = type of arg x
// nx = name of arg x
//
#define DEFINE_SYSCALL0(name) \
uint64_t sys_name##()

#define DEFINE_SYSCALL1(name, t1, n1) \
  uint64_t sys_name##(struct trap_frame *tf) {
    return _sys_name##((t1)tf->a0)\
  } \
  uint64_t _sys_name##(t1 n1)

#define DEFINE_SYSCALL2(name, t1, n1, t2, n2) \
  uint64_t sys_name##(struct trap_frame *tf) {
    return _sys_name##((t1)tf->a0, (t2)tf->a1)\
  } \
  uint64_t _sys_name##(t1 n1, t2 n2)

#define DEFINE_SYSCALL3(name, t1, n1, t2, n2, t3, n3) \
  uint64_t sys_name##(struct trap_frame *tf) {
    return _sys_name##((t1)tf->a0, (t2)tf->a1, (t3)tf->a2)\
  } \
  uint64_t _sys_name##(t1 n1, t2 n2, t3 n3)

#define DEFINE_SYSCALL4(name, t1, n1, t2, n2, t3, n3, t4, n4) \
  uint64_t sys_name##(struct trap_frame *tf) {
    return _sys_name##((t1)tf->a0, (t2)tf->a1, (t3)tf->a2, (t4)tf->a3)\
  } \
  uint64_t _sys_name##(t1 n1, t2 n2, t3 n3, t4 n4)
#endif
