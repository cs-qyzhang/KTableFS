#include <stdlib.h>
#include "util/freelist.h"
#include "util/arena.h"

#ifdef DEBUG
#include <assert.h>
#define Assert(expr)  assert(expr)
#else // DEBUG
#define Assert(expr)
#endif

struct list_node {
  int index;
  struct list_node* next;
};

struct freelist {
  struct list_node* head;
  struct arena* arena;
};

struct freelist* freelist_new(struct arena* arena) {
  Assert(arena);
  struct freelist* list = arena_allocate(arena, sizeof(*list));
  list->arena = arena;
  list->head = arena_allocate(arena, sizeof(*(list->head)));
  list->head->next = NULL;
  list->head->index = -1;
  return list;
}

void freelist_add(struct freelist* list, int index) {
  struct list_node* new_node = arena_allocate(list->arena, sizeof(*new_node));
  new_node->index = index;
  new_node->next = list->head->next;
  list->head->next = new_node;
}

int freelist_get(struct freelist* list) {
  if (list->head->next) {
    int index = list->head->next->index;
    list->head->next = list->head->next->next;
    return index;
  } else {
    return -1;
  }
}