#ifndef KTABLEFS_UTIL_MEMSLAB_H_
#define KTABLEFS_UTIL_MEMSLAB_H_

#include "util/arena.h"
#include "util/freelist.h"

struct memslab {
  struct arena* arena;
  struct freelist* freelist;
  int slab_size;
};

struct memslab* memslab_new(int slab_size);
void memslab_destroy(struct memslab*);
void* memslab_alloc(struct memslab*);
void memslab_free(struct memslab*, void*);

#endif // KTABLEFS_UTIL_MEMSLAB_H_