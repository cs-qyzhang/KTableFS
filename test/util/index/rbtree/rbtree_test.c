#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "util/rbtree.h"
#include "kvengine/kvengine.h"

int struct_key_comparator(void* a, void* b) {
  return (int)(uintptr_t)a - (int)(uintptr_t)b;
}

void tree_scan(void* key, void* value, void** data) {
  *(int*)data += (int)(uintptr_t)value;
}

int main(void) {
  struct rbtree* tree;
  tree = rbtree_new(struct_key_comparator);

  for (int i = 0; i < 1000000; ++i) {
    rbtree_insert(tree, (void*)(uintptr_t)i, (void*)(uintptr_t)(i + 1));
  }

  for (int i = 0; i < 1000000; ++i) {
    int value = (uintptr_t)rbtree_lookup(tree, (void*)(uintptr_t)i);
    assert(value == i + 1);
  }

  int sum = 0;
  int ret = rbtree_scan(tree, (void*)(uintptr_t)100, (void*)(uintptr_t)10000, tree_scan, &sum);
  assert(ret == (10000 - 100));
  int result = 0;
  for (int i = 100; i < 10000; ++i)
    result += i + 1;
  assert(sum == result);

  for (int i = 0; i < 1000000; ++i) {
    rbtree_remove(tree, (void*)(uintptr_t)i);
  }

  for (int i = 0; i < 1000000; ++i) {
    int* value = rbtree_lookup(tree, (void*)(uintptr_t)i);
    assert(value == NULL);
  }

  rbtree_destroy(tree);

  return 0;
}