#include <stdlib.h>
#include "util/arena.h"
#include "util/freelist.h"
#include "util/memslab.h"

struct memslab* memslab_new(int size) {
  struct arena* arena = arena_new();
  struct memslab* memslab = arena_allocate(arena, sizeof(*memslab));
  memslab->arena = arena;
  memslab->freelist = freelist_new(arena);
  return memslab;
}

void memslab_destroy(struct memslab* memslab) {
  arena_destroy(memslab->arena);
}

void* memslab_alloc(struct memslab* memslab) {
  if (memslab->arena == NULL) {
    memslab->arena = arena_new();
    memslab->freelist = freelist_new(memslab->arena);
  }
  void* data = freelist_get(memslab->freelist);
  if (data == NULL)
    data = arena_allocate(memslab->arena, memslab->slab_size);
  return data;
}

void memslab_free(struct memslab* memslab, void* data) {
  if (memslab->arena == NULL) {
    memslab->arena = arena_new();
    memslab->freelist = freelist_new(memslab->arena);
  }
  freelist_add(memslab->freelist, data);
}