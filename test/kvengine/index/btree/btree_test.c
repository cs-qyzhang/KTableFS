#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "btree.h"

int main(void) {
  struct btree* tree;
  tree = btree_new(4, key_comparator);

  for (int i = 0; i < 1000000; ++i) {
    int* value = malloc(sizeof(*value));
    struct key* key = malloc(sizeof(*key));
    key->val = i;
    key->data = malloc(sizeof(1));
    key->data[0] = '\0';
    *value = i;
    btree_insert(tree, key, value);
  }

  struct key* key = malloc(sizeof(*key));
  key->data = malloc(sizeof(1));
  key->data[0] = '\0';
  for (int i = 0; i < 1000000; ++i) {
    key->val = i;
    int* value = btree_lookup(tree, key);
    assert(value && *value == i);
  }

  for (int i = 0; i < 1000000; ++i) {
    key->val = i;
    btree_remove(tree, key);
  }

  for (int i = 0; i < 1000000; ++i) {
    key->val = i;
    int* value = btree_lookup(tree, key);
    assert(value == NULL);
  }

  btree_destroy(tree);

  return 0;
}