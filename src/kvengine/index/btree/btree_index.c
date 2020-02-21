#include "btree.h"
#include "kvengine/index.h"

struct index* index_new() {
  return (struct index*)btree_new(4);
}

void index_destroy(struct index* index) {
  btree_destroy((struct btree*)index);
}

int index_insert(struct index* index, hash_t key, void* value) {
  return btree_insert((struct btree*)index, key, value);
}

void* index_lookup(struct index* index, hash_t key) {
  return btree_lookup((struct btree*)index, key);
}

int index_remove(struct index* index, hash_t key) {
  return btree_remove((struct btree*)index, key);
}