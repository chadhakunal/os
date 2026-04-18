#ifndef LIST_H
#define LIST_H

#include "types.h"

struct list_node {
  struct list_node *next; 
  struct list_node *prev;
};

static inline bool list_is_empty(struct list_node *sentinel) {
  return sentinel->next == sentinel;
}

static inline void list_append(struct list_node *sentinel, struct list_node *new_node) {
  struct list_node *tail = sentinel->prev;
  tail->next = new_node;
  new_node->prev = tail;
  new_node->next = sentinel;
  sentinel->prev = new_node;
}

#define list_for_each(sentinel, pos) \
  for (struct list_node *pos = sentinel->next; pos != sentinel; pos = pos->next)

#define offset_of(type, member) ((size_t)(&((type *)0)->member))

#define container_of(node, type, member) \
  ((type *)((uint8_t *)node - offset_of(type, member)))
  
#endif 
