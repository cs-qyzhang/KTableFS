#ifndef KTABLEFS_UTIL_RBTREE
#define KTABLEFS_UTIL_RBTREE

#include "util/index.h"

struct rbtree;

struct rbtree* rbtree_new(key_cmp_t key_cmp);
void rbtree_destroy(struct rbtree* tree);
int rbtree_insert(struct rbtree* tree, void* key, void* value);
int rbtree_remove(struct rbtree* tree, void* key);
void* rbtree_lookup(struct rbtree* tree, void* key);
int rbtree_scan(struct rbtree* tree, void* min_key, void* max_key, scan_t scan, void* scan_arg);

#endif // KTABLEFS_UTIL_RBTREE