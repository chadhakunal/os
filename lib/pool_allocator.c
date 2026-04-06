#include "lib/pool_allocator.h"
#include "kernel/memory/page_allocator.h"

void *pool_alloc(struct pool *allocating_pool) {
  if (allocating_pool->free_list == NULL) {
    void *free_page = get_page(true);
    uint64_t num_objs = DEFAULT_PAGE_SIZE  / allocating_pool->obj_size;
    for (uint64_t i = 0; i < num_objs; i++) {
      struct pool_node *pn = (struct pool_node *) free_page + (i + allocating_pool->obj_size);
      if (allocating_pool->free_list == NULL) {
        allocating_pool->free_list = pn;
        pn->next = NULL;
      } else {
        pn->next = allocating_pool->free_list;
        allocating_pool->pool_node = pn;
      }
    }
  }
  void *free_space = allocating_pool->free_list;
  allocating_pool->free_list = allocating_pool->free_list->next;
  return free_space;
}

void pool_free(struct pool *allocating_pool, void *obj) {
  struct pool_node *pn = (struct pool_node *)obj;
  pn->next = allocating_pool->free_list;
  allocating_pool->free_list = pn;
}
