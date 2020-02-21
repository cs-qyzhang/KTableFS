#ifndef KTABLEFS_KVENGINE_SLAB_H_
#define KTABLEFS_KVENGINE_SLAB_H_

#include <sys/types.h>

struct arena;
struct slab {
  struct pagecache* pgcache;
  struct freelist* freelist;
  struct arena* arena;
  // used slab slot count
  size_t used;
  // slab slot size
  size_t slab_size;
  int max_index;
  int fd;
};

void* slab_read_item(struct slab* slab, int idx);
int slab_write_item(struct slab* slab, int idx, void* item, size_t size);

#endif // KTABLEFS_KVENGINE_SLAB_H_