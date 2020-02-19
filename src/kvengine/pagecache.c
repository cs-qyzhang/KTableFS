#include <stdlib.h>
#include <assert.h>
#include "kvengine/pagecache.h"
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
  pgcache->newest = NULL;
  pgcache->oldest = NULL;
  pgcache->used_page = 0;
}