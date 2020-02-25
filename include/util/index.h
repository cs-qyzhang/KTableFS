#ifndef KTABLEFS_KVENGINE_INDEX_H_
#define KTABLEFS_KVENGINE_INDEX_H_

typedef int (*key_cmp_t)(void*, void*);

struct index;

struct index* index_new(key_cmp_t comparator);
void index_destroy(struct index*);
int index_insert(struct index* index, void* key, void* value);
void* index_lookup(struct index* index, void* key);
int index_remove(struct index* index, void* key);

#endif // KTABLEFS_KVENGINE_INDEX_H_