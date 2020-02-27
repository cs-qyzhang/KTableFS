#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "util/btree.h"
#include "kvengine/kvengine.h"

struct key {
  int val;
  char* data;
};

int struct_key_comparator(void* a, void* b) {
  struct key* key1 = a;
  struct key* key2 = b;
  if (key1->val == key2->val) {
    return strcmp(key1->data, key2->data);
  } else {
    return (key1->val - key2->val);
  }
}

int main(void) {
  struct btree* tree;
  tree = btree_new(4, struct_key_comparator);

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