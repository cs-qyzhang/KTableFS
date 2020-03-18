#ifndef KVENGINE_UTIL_HASH_TABLE_H_
#define KVENGINE_UTIL_HASH_TABLE_H_

#include "util/hash.h"

struct hash_table;

struct hash_table* hash_table_new(size_t table_size);
void hash_table_destroy(struct hash_table* table);
void* hash_table_find(struct hash_table* table, hash_t hash, void** key);
void hash_table_insert(struct hash_table* table, void* key, void* data);
void hash_table_remove(struct hash_table* table, hash_t hash);

#endif // KVENGINE_UTIL_HASH_TABLE_H_