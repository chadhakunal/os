#ifndef SYSCALL_MACROS_H
#define SYSCALL_MACROS_H

#include "types.h"
// tx = type of arg x
// nx = name of arg x
//
#define DEFINE_SYSCALL0(name) \
  int64_t sys_##name(struct trap_frame *tf)

#define DEFINE_SYSCALL1(name, t1, n1) \
  int64_t _sys_##name(t1 n1); \
  int64_t sys_##name(struct trap_frame *tf) { \
    return _sys_##name((t1)tf->a0); \
  } \
  int64_t _sys_##name(t1 n1)

#define DEFINE_SYSCALL2(name, t1, n1, t2, n2) \
  int64_t _sys_##name(t1 n1, t2 n2); \
  int64_t sys_##name(struct trap_frame *tf) { \
    return _sys_##name((t1)tf->a0, (t2)tf->a1); \
  } \
  int64_t _sys_##name(t1 n1, t2 n2)

#define DEFINE_SYSCALL3(name, t1, n1, t2, n2, t3, n3) \
  int64_t _sys_##name(t1 n1, t2 n2, t3 n3); \
  int64_t sys_##name(struct trap_frame *tf) { \
    return _sys_##name((t1)tf->a0, (t2)tf->a1, (t3)tf->a2); \
  } \
  int64_t _sys_##name(t1 n1, t2 n2, t3 n3)

#define DEFINE_SYSCALL4(name, t1, n1, t2, n2, t3, n3, t4, n4) \
  int64_t _sys_##name(t1 n1, t2 n2, t3 n3, t4 n4); \
  int64_t sys_##name(struct trap_frame *tf) { \
    return _sys_##name((t1)tf->a0, (t2)tf->a1, (t3)tf->a2, (t4)tf->a3); \
  } \
  int64_t _sys_##name(t1 n1, t2 n2, t3 n3, t4 n4)

#define DEFINE_SYSCALL6(name, t1, n1, t2, n2, t3, n3, t4, n4, t5, n5, t6, n6) \
  int64_t _sys_##name(t1 n1, t2 n2, t3 n3, t4 n4, t5 n5, t6 n6); \
  int64_t sys_##name(struct trap_frame *tf) { \
    return _sys_##name((t1)tf->a0, (t2)tf->a1, (t3)tf->a2, (t4)tf->a3, (t5)tf->a4, (t6)tf->a5); \
  } \
  int64_t _sys_##name(t1 n1, t2 n2, t3 n3, t4 n4, t5 n5, t6 n6)

#endif
