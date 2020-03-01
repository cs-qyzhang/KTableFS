#include "util/btree.h"
#include "util/index.h"
#include "kvengine/kvengine.h"
#include "ktablefs_config.h"

struct index* index_new(key_cmp_t comparator) {
  return (struct index*)btree_new(BTREE_ORDER, comparator);
}

void index_destroy(struct index* index) {
  btree_destroy((struct btree*)index);
}

int index_insert(struct index* index, void* key, void* value) {
  return btree_insert((struct btree*)index, key, value);
}

void* index_lookup(struct index* index, void* key) {
  return btree_lookup((struct btree*)index, key);
}

int index_remove(struct index* index, void* key) {
  return btree_remove((struct btree*)index, key);
}

int index_scan(struct index* index, void* min_key, void* max_key, scan_t scan, void* scan_arg) {
  return btree_scan((struct btree*)index, min_key, max_key, scan, scan_arg);
}