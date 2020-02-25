#include <stdlib.h>
#include <string.h>
#include "util/hash.h"
#include "util/freelist.h"
#include "kvengine/io_worker.h"
#include "kvengine/slab.h"
#include "kvengine/pagecache.h"
#include "kvengine/io_context.h"
#include "ktablefs_config.h"

static inline int slab_get_page(struct slab* slab, int index) {
  return (slab->slab_size * index) / PAGE_SIZE;
}

static inline int slab_get_page_offset(struct slab* slab, int index) {
  return (slab->slab_size * index) % PAGE_SIZE;
}

void slab_read_item(struct slab* slab, int index, struct io_context* ctx) {
  int page_idx = slab_get_page(slab, index);
  int page_offset = slab_get_page_offset(slab, index);
  page_read(slab->pgcache, page_hash(slab->fd, page_idx), page_offset, ctx);
}

void slab_write_item(struct slab* slab, int index, void* item, size_t size, struct io_context* ctx) {
  int page_idx = slab_get_page(slab, index);
  int page_offset = slab_get_page_offset(slab, index);
  page_write(slab->pgcache, page_hash(slab->fd, page_idx), page_offset, item, size, ctx);
}

int slab_get_free_index(struct slab* slab) {
  int index = freelist_get(slab->freelist);
  if (index == -1) {
    index = slab->max_index++;
  }
  slab->used++;
  return index;
}

// callback function
void slab_remove_item(struct io_context* ctx) {
  ctx->thread_data->slab->used--;
  freelist_add(ctx->thread_data->slab->freelist, ctx->slab_index);
}