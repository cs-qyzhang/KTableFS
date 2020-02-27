#ifndef KTABLEFS_UTIL_BTREE_H_
#define KTABLEFS_UTIL_BTREE_H_

#include "util/index.h"

struct btree;

struct btree* btree_new(int order, key_cmp_t key_cmp);
void btree_destroy(struct btree* tree);
int btree_insert(struct btree* tree, void* key, void* value);
int btree_remove(struct btree* tree, void* key);
void* btree_lookup(struct btree* tree, void* key);

#endif // KTABLEFS_UTIL_BTREE_H_