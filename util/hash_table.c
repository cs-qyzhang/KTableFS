#include <stdlib.h>
#include "util/hash_table.h"

struct hash_table {
  void** key;
  void** data;
  size_t table_size;
};

struct hash_table* hash_table_new(size_t table_size) {
  struct hash_table* table = malloc(sizeof(*table));
  table->table_size = table_size;
  table->key = calloc(table_size, sizeof(void*));
  table->data = calloc(table_size, sizeof(void*));
  return table;
}

void hash_table_destroy(struct hash_table* table) {
  free(table->key);
  free(table->data);
  free(table);
}

void* hash_table_find(struct hash_table* table, hash_t hash, void** key) {
  int idx = hash % table->table_size;
  if (table->key[idx] && (*((hash_t*)(table->key[idx]))) == hash) {
    *key = table->key[idx];
    return table->data[idx];
  } else
    return NULL;
}

void hash_table_insert(struct hash_table* table, void* key, void* data) {
  int idx = (*(hash_t*)key) % table->table_size;
  table->key[idx] = key;
  table->data[idx] = data;
}

void hash_table_remove(struct hash_table* table, hash_t hash) {
  int idx = hash % table->table_size;
  table->key[idx] = NULL;
}