#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "kvengine/pagecache.h"
#include "kvengine/index.h"
#include "kvengine/io_context.h"
#include "kvengine/kv_request.h"
#include "kvengine/kv_event.h"
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

// callback function
void pagecache_insert_index(struct io_context* ctx) {
  index_insert(ctx->pgcache->index, ctx->lru->hash, ctx->lru);
}

void kv_event_enqueue(struct kv_event* event, int thread_idx);

void page_read(struct pagecache* pgcache, hash_t hash, size_t page_offset, void* data, size_t size, struct io_context* ctx) {
  struct lru_entry* lru;
  lru = index_lookup(pgcache->index, hash);
  if (lru) {
    void* item = &lru->page[page_offset];
    item_to_kv(item, NULL, &ctx->kv_event->value);
    ctx->kv_event->return_code = 0;
    kv_event_enqueue(ctx->kv_event, ctx->thread_index);
    return;
  } else {
    lru = pagecache_find_free_page_(pgcache);
    io_read_page(hash, lru->page);
    lru->hash = hash;
    lru->valid = 0;
    lru->dirty = 0;

    int fd;
    int page_index;
    get_page_from_hash(hash, &fd, &page_index);

    ctx->page_offset = page_offset;

    ctx->iocb = malloc(sizeof(*ctx->iocb));
    memset(ctx->iocb, 0, sizeof(*ctx->iocb));
    ctx->iocb->aio_buf = lru->page;
    ctx->iocb->aio_fildes = fd;
    ctx->iocb->aio_offset = PAGE_SIZE * page_index;
    ctx->iocb->aio_nbytes = PAGE_SIZE;
    ctx->iocb->aio_lio_opcode = IOCB_CMD_PREAD;
    io_context_insert_callback(ctx->do_at_io_wait, pagecache_insert_index);
    io_context_enqueue(ctx);
  }
}

// callback function
void pagecache_write(struct io_context* ctx) {
  memcpy(&ctx->lru->page[ctx->iocb->aio_offset], ctx->iocb->aio_buf, ctx->iocb->aio_nbytes);
}

void io_context_enqueue(struct io_context* ctx);

/*
 * write page to disk.
 * if page already in page cache, need to update data in page cache
 * if page doesn't in page cache, directly write to disk.
 */
void page_write(struct pagecache* pgcache, hash_t hash, size_t page_offset, void* data, size_t size, struct io_context* ctx) {
  struct lru_entry* lru;
  lru = index_lookup(pgcache->index, hash);
  if (lru) {
    ctx->lru = lru;
    io_context_insert_callback(ctx->do_at_io_wait, pagecache_write);
    lru_update_(pgcache, lru);
  }

  int fd;
  int page_index;
  get_page_from_hash(hash, &fd, &page_index);

  ctx->iocb = malloc(sizeof(*ctx->iocb));
  memset(ctx->iocb, 0, sizeof(*ctx->iocb));
  ctx->iocb->aio_buf = data;
  ctx->iocb->aio_fildes = fd;
  ctx->iocb->aio_offset = PAGE_SIZE * page_index;
  ctx->iocb->aio_nbytes = size;
  ctx->iocb->aio_lio_opcode = IOCB_CMD_PWRITE;
  io_context_enqueue(ctx);
}