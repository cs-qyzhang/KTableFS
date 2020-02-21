#include <stdlib.h>
#include <string.h>
#include "kvengine/slab.h"
#include "kvengine/pagecache.h"
#include "kvengine/hash.h"
#include "kvengine/freelist.h"
#include "ktablefs_config.h"

static inline int slab_get_page(struct slab* slab, int index) {
  return (slab->slab_size * index) / PAGE_SIZE;
}

static inline int slab_get_page_offset(struct slab* slab, int index) {
  return (slab->slab_size * index) % PAGE_SIZE;
}

static inline hash_t slab_page_hash(struct slab* slab, int page_index) {
  return ((hash_t)slab->fd << 40U) + (hash_t)page_index;
}

void* slab_read_item(struct slab* slab, int index) {
  int page_idx = slab_get_page(slab, index);
  int page_offset = slab_get_page_offset(slab, index);
  struct lru_entry* lru;
  lru = pagecache_lookup(slab->pgcache, slab_page_hash(slab, page_idx));
  Assert(lru && lru->page);
  char* data = malloc(slab->slab_size);
  memcpy(data, &lru->page[page_offset], slab->slab_size);
  return data;
}

int slab_write_item(struct slab* slab, int index, void* item, size_t size) {
  int page_idx = slab_get_page(slab, index);
  int page_offset = slab_get_page_offset(slab, index);
  struct lru_entry* lru;
  lru = pagecache_lookup(slab->pgcache, slab_page_hash(slab, page_idx));
  pagecache_write(lru, page_offset, item, size);
  return 0;
}

int slab_get_free_index(struct slab* slab) {
  int index = freelist_get(slab->freelist);
  if (index == -1) {
    index = slab->max_index++;
  }
  return index;
}