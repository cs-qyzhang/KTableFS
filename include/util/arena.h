#ifndef KTABLEFS_UTIL_ARENA_H_
#define KTABLEFS_UTIL_ARENA_H_

#include <sys/types.h>

#define PAGE_SIZE 4096

struct arena;

struct arena* arena_new();
void* arena_allocate(struct arena* arena_, size_t size);
void* arena_allocate_aligned(struct arena* arena_, size_t size, size_t align_size);
void arena_destroy(struct arena* arena_);

#endif // KTABLEFS_UTIL_ARENA_H_