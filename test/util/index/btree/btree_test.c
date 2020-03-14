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

void tree_scan(void* key, void* value, void** data) {
  *(int*)data += *(int*)value;
}

int main(void) {
  struct btree* tree;
  tree = btree_new(4, struct_key_comparator);

  for (int i = 0; i < 1000000; ++i) {
    int* value = malloc(sizeof(*value));
    struct key* key = malloc(sizeof(*key));
    key->val = i;
    key->data = malloc(1);
    key->data[0] = '\0';
    *value = i;
    btree_insert(tree, key, value);
  }

  struct key* key = malloc(sizeof(*key));
  key->data = malloc(1);
  key->data[0] = '\0';
  for (int i = 0; i < 1000000; ++i) {
    key->val = i;
    int* value = btree_lookup(tree, key);
    assert(value && *value == i);
  }

  struct key* min_key = malloc(sizeof(*min_key));
  min_key->data = malloc(1);
  min_key->data[0] = '\0';
  min_key->val = 100;
  struct key* max_key = malloc(sizeof(*max_key));
  max_key->data = malloc(1);
  max_key->data[0] = '\0';
  max_key->val = 10000;

  int sum = 0;
  int ret = btree_scan(tree, min_key, max_key, tree_scan, &sum);
  assert(ret == (10000 - 100 + 1));
  int result = 0;
  for (int i = 100; i <= 10000; ++i)
    result += i;
  assert(sum == result);

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