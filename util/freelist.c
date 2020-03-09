#include <stdlib.h>
#include "util/freelist.h"
#include "util/arena.h"
#include "debug.h"

struct list_node {
  void* data;
  struct list_node* next;
};

struct freelist {
  struct list_node* head;
  struct arena* arena;
};

struct freelist* freelist_new(struct arena* arena) {
  struct freelist* list;
  if (arena) {
    list = arena_allocate(arena, sizeof(*list));
    list->arena = arena;
    list->head = arena_allocate(arena, sizeof(*(list->head)));
  } else {
    list = malloc(sizeof(*list));
    list->arena = NULL;
    list->head = malloc(sizeof(*(list->head)));
  }
  list->head->next = NULL;
  list->head->data = NULL;
  return list;
}

void freelist_add(struct freelist* list, void* data) {
  struct list_node* new_node;
  if (list->arena) {
    new_node = arena_allocate(list->arena, sizeof(*new_node));
  } else {
    new_node = malloc(sizeof(*new_node));
  }
  new_node->data = data;
  new_node->next = list->head->next;
  list->head->next = new_node;
}

void* freelist_get(struct freelist* list) {
  if (list->head->next) {
    void* data = list->head->next->data;
    list->head->next = list->head->next->next;
    return data;
  } else {
    return NULL;
  }
}