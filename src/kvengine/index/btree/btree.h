#ifndef KTABLEFS_KVENGINE_BTREE_H_
#define KTABLEFS_KVENGINE_BTREE_H_

#include "kvengine/key.h"

struct btree;

struct btree* btree_new(int order, key_cmp_t key_cmp);
void btree_destroy(struct btree* tree);
int btree_insert(struct btree* tree, struct key* key, void* value);
int btree_remove(struct btree* tree, struct key* key);
void* btree_lookup(struct btree* tree, struct key* key);

#endif // KTABLEFS_KVENGINE_BTREE_H_