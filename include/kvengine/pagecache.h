#ifndef KTABLEFS_PAGECACHE_H_
#define KTABLEFS_PAGECACHE_H_

#include <sys/types.h>

struct lru_entry {
  struct lru* prev;
  struct lru* next;
  char* page;
  int valid;
  int dirty;
};

struct pagecache {
  struct lru_entry* newest;
  struct lru_entry* oldest;
  char* data;
  size_t used_page;
  size_t max_page;
};

#endif // KTABLEFS_PAGECACHE_H_