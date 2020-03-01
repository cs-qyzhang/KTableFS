#ifndef KTABLEFS_KVENGINE_PAGECACHE_H_
#define KTABLEFS_KVENGINE_PAGECACHE_H_

#include <stddef.h>
#include "util/hash.h"

struct index;

struct lru_entry {
  struct lru_entry* prev;
  struct lru_entry* next;
  char* page;
  hash_t hash;
  int valid;
  int dirty;
};

struct pagecache {
  struct lru_entry* newest;
  struct lru_entry* oldest;
  char* data;
  struct index* index;
  size_t used_page;
  size_t max_page;
};

struct io_context;
struct pagecache* pagecache_new(size_t page_nr);
void page_write(struct pagecache* pgcache, hash_t hash, size_t page_offset, void* data, size_t size, struct io_context* ctx);
void page_read(struct pagecache* pgcache, hash_t hash, size_t page_offset, struct io_context* ctx);
void* page_read_sync(struct pagecache* pgcache, hash_t hash, size_t page_offset);

#endif // KTABLEFS_KVENGINE_PAGECACHE_H_