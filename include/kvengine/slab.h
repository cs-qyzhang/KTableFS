#ifndef KTABLEFS_KVENGINE_SLAB_H_
#define KTABLEFS_KVENGINE_SLAB_H_

#include <sys/types.h>

struct io_context;
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

void slab_read_item(struct slab* slab, int index, struct io_context* ctx);
void slab_write_item(struct slab* slab, int index, void* item, size_t size, struct io_context* ctx);
void slab_remove_item(struct io_context* ctx);
int slab_get_free_index(struct slab* slab);

#endif // KTABLEFS_KVENGINE_SLAB_H_