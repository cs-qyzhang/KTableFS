#include "util/rbtree.h"
#include "util/index.h"

struct index* index_new(key_cmp_t comparator) {
  return (struct index*)rbtree_new(comparator);
}

void index_destroy(struct index* index) {
  rbtree_destroy((struct rbtree*)index);
}

int index_insert(struct index* index, void* key, void* value) {
  return rbtree_insert((struct rbtree*)index, key, value);
}

void* index_lookup(struct index* index, void* key) {
  return rbtree_lookup((struct rbtree*)index, key);
}

int index_remove(struct index* index, void* key) {
  return rbtree_remove((struct rbtree*)index, key);
}

int index_scan(struct index* index, void* min_key, void* max_key, scan_t scan, void* scan_arg) {
  return rbtree_scan((struct rbtree*)index, min_key, max_key, scan, scan_arg);
}