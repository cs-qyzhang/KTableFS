#include <stdlib.h>
#include <assert.h>
#include "kvengine/pagecache.h"
#include "kvengine/index.h"
#include "ktablefs_config.h"

struct pagecache* pagecache_new(size_t page_nr) {
  assert(page_nr > 0);
  struct pagecache* pgcache = malloc(sizeof(struct pagecache));
  pgcache->max_page = page_nr;
  pgcache->data = aligned_alloc(PAGE_SIZE, PAGE_SIZE * page_nr);
  if (pgcache->data == NULL) {
    // TODO
    die("page cache malloc failed! please try a smaller page cache size!");
    assert(pgcache->data);
  }
  pgcache->index = index_new();
  if (pgcache->index == NULL) {
    // TODO
    die("page cache can't build index!");
  }
  pgcache->newest = NULL;
  pgcache->oldest = NULL;
  pgcache->used_page = 0;
  return pgcache;
}

/*
 * make lru the newest in page cache
 */
static void lru_update_(struct pagecache* pgcache, struct lru_entry* lru) {
  if (pgcache->newest == lru) {
    return;
  }
  // lru is an old entry
  if (lru->next) {
    lru->prev->next = lru->next;
    lru->next->prev = lru->prev;
  }
  if (pgcache->newest) {
    lru->next = pgcache->newest;
    lru->prev = pgcache->oldest;
  } else {
    lru->next = lru;
    lru->prev = lru;
  }
  pgcache->newest = lru;
  pgcache->oldest = lru->prev;
}

/*
 * find a free page in page cache
 *
 * if there is no free page avaliable, it will use the oldest page.
 * if the oldest page is dirty, it will write this page to disk.
 */
static struct lru_entry* pagecache_find_free_page_(struct pagecache* pgcache) {
  struct lru_entry* lru;
  if (pgcache->used_page == pgcache->max_page) {
    // page cache full, use the oldest page
    lru = pgcache->oldest;
    index_remove(pgcache->index, lru->hash);
    if (lru->dirty) {
      io_write_page(lru->hash, lru->page);
    }
  } else {
    // page cache has free page
    // TODO: use arena?
    lru = malloc(sizeof(struct lru_entry));
    lru->dirty = 0;
    lru->next = NULL;
    lru->prev = NULL;
    lru->valid = 0;
  }
  return lru;
}

/*
 * lookup page in page cache use its hash
 *
 * if the page doesn't in page cache, it will read page from disk.
 */
struct lru_entry* pagecache_lookup(struct pagecache* pgcache, hash_t hash) {
  struct lru_entry* lru;
  lru = index_lookup(pgcache->index, hash);
  if (lru) {
    Assert(lru->hash == hash);
  } else {
    lru = pagecache_find_free_page_(pgcache);
    io_read_page(hash, lru->page);
    lru->hash = hash;
    lru->valid = 1;
    lru->dirty = 0;
    index_insert(pgcache->index, hash, lru);
  }
  lru_update_(pgcache, lru);
  return lru;
}