#ifndef KTABLEFS_KVENGINE_INDEX_H_
#define KTABLEFS_KVENGINE_INDEX_H_

#include <sys/types.h>
#include "kvengine/hash.h"

struct index;

struct index* index_new();
void index_destroy(struct index*);
int index_insert(struct index* index, hash_t key, void* value);
void* index_lookup(struct index* index, hash_t key);
int index_remove(struct index* index, hash_t key);

#endif // KTABLEFS_KVENGINE_INDEX_H_