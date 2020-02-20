#ifndef KTABLEFS_KVENGINE_BTREE_H_
#define KTABLEFS_KVENGINE_BTREE_H_

#include "kvengine/hash.h"

struct btree;

struct btree* btree_new(int order);
void btree_destroy(struct btree* tree);
int btree_insert(struct btree* tree, hash_t key, void* value);
int btree_remove(struct btree* tree, hash_t key);
void* btree_lookup(struct btree* tree, hash_t key);

#endif // KTABLEFS_KVENGINE_BTREE_H_