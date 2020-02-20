#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "btree.h"

int main(void) {
  struct btree* tree;
  tree = btree_new(4);

  for (int i = 0; i < 1000; ++i) {
    int* value = malloc(sizeof(*value));
    *value = i;
    btree_insert(tree, i, value);
  }

  for (int i = 0; i < 1000; ++i) {
    int* value = btree_lookup(tree, i);
    assert(value && *value == i);
  }

  for (int i = 1000; i < 4000; ++i) {
    int* value = btree_lookup(tree, i);
    assert(value == NULL);
  }

  for (int i = 0; i < 1000; i += 2) {
    btree_remove(tree, i);
  }

  for (int i = 0; i < 1000; i += 2) {
    int* value = btree_lookup(tree, i);
    assert(value == NULL);
  }

  for (int i = 1; i < 1000; i += 2) {
    int* value = btree_lookup(tree, i);
    assert(value && *value == i);
  }

  for (int i = 1000; i < 4000; ++i) {
    int* value = btree_lookup(tree, i);
    assert(value == NULL);
  }

  btree_destroy(tree);

  return 0;
}