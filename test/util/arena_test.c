#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include "util/arena.h"

static void fill(char* ptr, size_t size) {
  for (int i = 0; i < size; ++i) {
    ptr[i] = i % 0x100;
  }
}

int main(void) {
  struct arena* arena = arena_new();
  char* s1;
  char* s2;
  s1 = arena_allocate(arena, 500);
  fill(s1, 500);
  s2 = arena_allocate(arena, 100);
  fill(s2, 100);
  assert(s2 == s1 + 500);

  srand(time(NULL));
  // small memory allocation test
  for (int i = 0; i < 10000; ++i) {
    size_t size = rand() % 0x400 + 1;
    s1 = arena_allocate(arena, size);
    fill(s1, size);
  }

  // random memory allocation test
  for (int i = 0; i < 10000; ++i) {
    size_t size = rand() % 0x2000 + 1;
    s1 = arena_allocate(arena, size);
    fill(s1, size);
  }

  // aligned random memory allocation test
  for (int i = 0; i < 10000; ++i) {
    size_t size = rand() % 0x2000 + 1;
    s1 = arena_allocate_aligned(arena, size, 16);
    assert(((uintptr_t)s1 & 0xFU) == 0);
    fill(s1, size);
  }

  arena_destroy(arena);

  return 0;
}