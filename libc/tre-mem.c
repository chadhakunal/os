/*
 * TRE allocator (from musl's tre-mem.c, adapted for sbunix).
 * Original by Ville Laurikari; see regcomp.c for license.
 */

#include "tre.h"
#include <stdlib.h>
#include <string.h>

tre_mem_t tre_mem_new_impl(int provided, void *provided_block) {
  (void)provided;
  (void)provided_block;
  tre_mem_t mem = malloc(sizeof(*mem));
  if (!mem)
    return NULL;
  mem->blocks = NULL;
  mem->current = NULL;
  mem->ptr = NULL;
  mem->n = 0;
  mem->failed = 0;
  mem->provided = NULL;
  return mem;
}

void *tre_mem_alloc_impl(tre_mem_t mem, int provided, void *provided_block,
                         int zero, size_t size) {
  void *ptr;

  (void)provided;
  (void)provided_block;

  if (!mem || mem->failed)
    return NULL;

  ptr = zero ? calloc(1, size) : malloc(size);
  if (!ptr) {
    mem->failed = 1;
    return NULL;
  }

  tre_list_t *bl = malloc(sizeof(*bl));
  if (!bl) {
    free(ptr);
    mem->failed = 1;
    return NULL;
  }
  bl->data = ptr;
  bl->next = mem->blocks;
  mem->blocks = bl;
  return ptr;
}

void tre_mem_destroy(tre_mem_t mem) {
  tre_list_t *bl;

  if (!mem)
    return;

  for (bl = mem->blocks; bl;) {
    tre_list_t *next = bl->next;
    free(bl->data);
    free(bl);
    bl = next;
  }
  free(mem);
}
