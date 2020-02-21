#ifndef KTABLEFS_KVENGINE_PAGECACHE_H_
#define KTABLEFS_KVENGINE_PAGECACHE_H_

#include <sys/types.h>
#include "kvengine/hash.h"

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

struct pagecache* pagecache_new(size_t page_nr);
struct lru_entry* pagecache_lookup(struct pagecache* pgcache, hash_t hash);
inline void pagecache_write(struct lru_entry* lru, int page_offset,
                            void* data, size_t size);

#endif // KTABLEFS_KVENGINE_PAGECACHE_H_