#ifndef POOL_ALLOCATOR_H
#define POOL_ALLOCATOR_H

#include "types.h"

struct pool_node {
  struct pool_node *next;
}

struct pool {
  struct pool_node *free_list;
  uint64_t obj_size;
}

void *pool_alloc(struct pool);
void pool_free(void *obj);

#define DEFINE_POOL(name, type) \
  static struct pool name##_pool = { NULL, sizeof(type)}; \
  \
  static inline type *name##_alloc(&name##_pool) { \
    return (type *)pool_alloc(&name##_pool); \
  } \
  \
  static inline name##_free(type *freeing_obj) { \
    pool_free(&name##_pool, (void *)freeing_obj); \
  }

#endif
