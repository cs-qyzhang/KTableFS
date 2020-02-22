#ifndef KTABLEFS_KVENGINE_INDEX_H_
#define KTABLEFS_KVENGINE_INDEX_H_

#include <sys/types.h>
#include "kvengine/key.h"

struct index;

struct index* index_new();
void index_destroy(struct index*);
int index_insert(struct index* index, struct key* key, void* value);
void* index_lookup(struct index* index, struct key* key);
int index_remove(struct index* index, struct key* key);

#endif // KTABLEFS_KVENGINE_INDEX_H_