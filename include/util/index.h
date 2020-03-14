#ifndef KTABLEFS_UTIL_INDEX_H_
#define KTABLEFS_UTIL_INDEX_H_

typedef int (*key_cmp_t)(void* key1, void* key2);
typedef void (*scan_t)(void* key, void* value, void** scan_arg);

struct index;

struct index* index_new(key_cmp_t comparator);
void index_destroy(struct index*);
int index_insert(struct index* index, void* key, void* value);
void* index_lookup(struct index* index, void* key);
int index_remove(struct index* index, void* key);
int index_scan(struct index* index, void* min_key, void* max_key, scan_t scan, void* scan_arg);

#endif // KTABLEFS_UTIL_INDEX_H_