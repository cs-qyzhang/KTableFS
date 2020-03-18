#define _GNU_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "util/index.h"
#include "kvengine/kvengine.h"
#include "kvengine/pagecache.h"
#include "kvengine/io_context.h"
#include "kvengine/io_worker.h"
#include "ktablefs_config.h"
#include "debug.h"

int hash_t_comparator(void* a, void* b) {
  hash_t hash1 = *(hash_t*)a;
  hash_t hash2 = *(hash_t*)b;
  return (hash1 < hash2) ? -1 : (hash1 == hash2 ? 0 : 1);
}

struct pagecache* pagecache_new(size_t page_nr) {
  assert(page_nr > 0);
  struct pagecache* pgcache = malloc(sizeof(struct pagecache));
  pgcache->max_page = page_nr;
  pgcache->data = aligned_alloc(PAGE_SIZE, PAGE_SIZE * page_nr);
  if (pgcache->data == NULL) {
    // TODO
    //die("page cache malloc failed! please try a smaller page cache size!");
    assert(pgcache->data);
  }
  pgcache->index = index_new(hash_t_comparator);
  if (pgcache->index == NULL) {
    // TODO
    //die("page cache can't build index!");
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
    index_remove(pgcache->index, &lru->hash);
  } else {
    // page cache has free page
    // TODO: use arena?
    lru = malloc(sizeof(struct lru_entry));
    lru->dirty = 0;
    lru->next = NULL;
    lru->prev = NULL;
    lru->valid = 0;
    lru->page = pgcache->data + (pgcache->used_page++ * PAGE_SIZE);
  }
  return lru;
}

// callback function
void pagecache_insert_index(struct io_context* ctx) {
  hash_t* hash = malloc(sizeof(*hash));
  *hash = ctx->lru->hash;
  index_insert(ctx->thread_data->pagecache->index, hash, ctx->lru);
  lru_update_(ctx->thread_data->pagecache, ctx->lru);
}

void io_context_enqueue(struct io_context* ctx);

void* page_read_sync(struct pagecache* pgcache, hash_t hash, size_t page_offset) {
  struct lru_entry* lru;
  hash_t* h = malloc(sizeof(hash_t));
  *h = hash;
  lru = index_lookup(pgcache->index, h);
  if (!lru) {
    lru = pagecache_find_free_page_(pgcache);
    lru->hash = hash;
    lru->valid = 1;
    lru->dirty = 0;

    int fd;
    int page_index;
    get_page_from_hash(hash, &fd, &page_index);

    ssize_t ret = pread(fd, lru->page, PAGE_SIZE, PAGE_SIZE * page_index);
    assert(ret != -1);
  }
  lru_update_(pgcache, lru);
  return &lru->page[page_offset];
}

void page_read(struct pagecache* pgcache, hash_t hash, size_t page_offset,
               struct io_context* ctx) {
  struct lru_entry* lru;
  hash_t* h = malloc(sizeof(hash_t));
  *h = hash;
  lru = index_lookup(pgcache->index, h);
  if (lru) {
    void* item = &lru->page[page_offset];
    item_to_value(item, &ctx->respond.value);
    ctx->respond.res = 0;
    kv_finish(ctx->batch, &ctx->respond);
    lru_update_(pgcache, lru);
  } else {
    lru = pagecache_find_free_page_(pgcache);
    lru->hash = hash;

    int fd;
    int page_index;
    get_page_from_hash(hash, &fd, &page_index);

    ctx->lru = lru;
    ctx->page_offset = page_offset;

    ctx->iocb = malloc(sizeof(*ctx->iocb));
    memset(ctx->iocb, 0, sizeof(*ctx->iocb));
    ctx->iocb->aio_buf = (uintptr_t)lru->page;
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
  memcpy(&ctx->lru->page[ctx->page_offset], (void*)(uintptr_t)ctx->iocb->aio_buf,
         ctx->iocb->aio_nbytes);
}

/*
 * write page to disk.
 * if page already in page cache, need to update data in page cache
 * if page doesn't in page cache, directly write to disk.
 */
void page_write(struct pagecache* pgcache, hash_t hash, size_t page_offset,
                void* data, size_t size, struct io_context* ctx) {

  struct lru_entry* lru;
  hash_t* h = malloc(sizeof(hash_t));
  *h = hash;
  lru = index_lookup(pgcache->index, h);
  if (lru) {
    ctx->lru = lru;
    ctx->page_offset = page_offset;
    io_context_insert_callback(ctx->do_at_io_wait, pagecache_write);
    lru_update_(pgcache, lru);
  }

  int fd;
  int page_index;
  get_page_from_hash(hash, &fd, &page_index);

  ctx->iocb = malloc(sizeof(*ctx->iocb));
  memset(ctx->iocb, 0, sizeof(*ctx->iocb));
  ctx->iocb->aio_buf = (uintptr_t)data;
  ctx->iocb->aio_fildes = fd;
  ctx->iocb->aio_offset = PAGE_SIZE * page_index + page_offset;
  ctx->iocb->aio_nbytes = size;
  ctx->iocb->aio_lio_opcode = IOCB_CMD_PWRITE;
  io_context_enqueue(ctx);
}