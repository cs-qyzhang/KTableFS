#include <stdlib.h>
#include "arena.h"
#include "kvengine/pagecache.h"
#include "kvengine/queue.h"
#include "kvengine/index.h"
#include "kvengine/aio-wrapper.h"
#include "kvengine/io_context.h"
#include "kvengine/kv_request.h"
#include "kvengine/kv_event.h"
#include "kvengine/slab.h"
#include "ktablefs_config.h"

struct queue** kv_request_queues;
struct index** indexs;
struct queue** kv_event_queues;
struct queue** io_context_queues;
struct arena** arenas;
struct slab** slabs;
int slab_nr;

static inline void nop() {
  for (volatile int i = 0; i < 10000; ++i) ;
}

static struct slab* get_slab_by_key(struct key* key) {
  for (int i = 0; i < slab_nr; ++i) {
    if (key->length <= slabs[i]->slab_size - sizeof(struct item_head) - sizeof(struct value)) {
      return slabs[i];
    }
  }
  assert(0);
  return NULL;
}

void io_context_enqueue(struct io_context* ctx) {
  enqueue(io_context_queues[ctx->thread_index], ctx);
}

void kv_event_enqueue(struct kv_event* event, int thread_idx) {
  enqueue(kv_event_queues[thread_idx], event);
}

// callback function
void index_insert_wrapper(struct io_context* ctx) {
  index_insert(indexs[ctx->thread_index], ctx->kv_request->key, ctx->slab_index);
}

// callback function
void kv_add_finish(struct io_context* ctx) {
  ctx->kv_event->return_code = 0;
  kv_event_enqueue(ctx->kv_event, ctx->thread_index);
}

// callback function
void kv_get_finish(struct io_context* ctx) {
  ctx->lru->valid = 1;
  void* item = &ctx->lru->page[ctx->page_offset];
  item_to_kv(item, NULL, &ctx->kv_event->value);
  ctx->kv_event->return_code = 0;
  kv_event_enqueue(ctx->kv_event, ctx->thread_index);
}

void process_add_request(struct kv_request* req, int thread_idx) {
  struct index* index = indexs[thread_idx];
  struct queue* event_que = kv_event_queues[thread_idx];
  void* val = index_lookup(index, req->key);
  if (val) {
    // error! key already in!
    struct kv_event* event = malloc(sizeof(*event));
    event->sequence = req->sequence;
    event->return_code = -1;
    kv_event_enqueue(event, thread_idx);
    return;
  }

  void* item;
  size_t item_size = kv_to_item(req->key, req->value, &item);
  struct slab* slab = get_slab_by_key(req->key);
  int slab_idx = slab_get_free_index(slab);

  struct io_context* ctx = malloc(sizeof(*ctx));
  memset(ctx, 0, sizeof(*ctx));
  ctx->pgcache = slab->pgcache;
  ctx->thread_index = thread_idx;
  ctx->kv_request = req;
  ctx->kv_event = malloc(sizeof(*ctx->kv_event));
  ctx->kv_event->sequence = ctx->kv_request->sequence;
  ctx->slab_index = slab_idx;
  io_context_insert_callback(ctx->do_at_io_wait, index_insert_wrapper);
  io_context_insert_callback(ctx->do_at_io_finish, kv_add_finish);

  slab_write_item(slab, slab_idx, item, item_size, ctx);
}

void process_get_request(struct kv_request* req, int thread_idx) {
  struct index* index = indexs[thread_idx];
  struct queue* event_que = kv_event_queues[thread_idx];
  void* val = index_lookup(index, req->key);
  if (!val) {
    struct kv_event* event = malloc(sizeof(*event));
    event->sequence = req->sequence;
    event->return_code = -1;
    enqueue(event_que, event);
    return;
  }

  int slab_idx = (int)val - 1;
  struct slab* slab = get_slab_by_key(req->key);

  struct io_context* ctx = malloc(sizeof(*ctx));
  memset(ctx, 0, sizeof(*ctx));
  ctx->pgcache = slab->pgcache;
  ctx->thread_index = thread_idx;
  ctx->kv_request = req;
  ctx->kv_event = malloc(sizeof(*ctx->kv_event));
  ctx->kv_event->sequence = ctx->kv_request->sequence;
  io_context_insert_callback(ctx->do_at_io_finish, kv_get_finish);

  slab_read_item(slab, slab_idx, ctx);
}

int worker_deque_request(int thread_idx) {
  struct queue* kv_que = kv_request_queues[thread_idx];

  while (queue_empty(kv_que)) {
    nop();
  }

  int req_nr = 0;
  while (!queue_empty(kv_que)) {
    struct kv_request* req = dequeue(kv_que);
    Assert(req);
    switch (req->type) {
      case ADD:
        process_add_request(req, thread_idx);
        break;
      case GET:
        process_get_request(req, thread_idx);
        break;
      case UPDATE:
        break;
      case DELETE:
        break;
      case SCAN:
        break;
      default:
        Assert(0);
        break;
    }
    req_nr++;
  }
  return req_nr;
}

void* worker_thread_main(void* arg) {
  int thread_idx = *(int*)arg;

  struct io_context* io_ctx[AIO_MAX_EVENTS];
  struct iocb** iocbs = malloc(sizeof(struct iocb*) * AIO_MAX_EVENTS);
  struct io_event* io_events = malloc(sizeof(struct io_event) * AIO_MAX_EVENTS);
  struct queue* io_que = io_context_queues[thread_idx];

  aio_context_t aio_ctx;
  io_setup(AIO_MAX_EVENTS, &aio_ctx);

  while (1) {
    worker_deque_request(thread_idx);

    int io_ctx_nr = 0;
    while (!queue_empty(io_que)) {
      io_ctx[io_ctx_nr] = dequeue(io_que);
      iocbs[io_ctx_nr] = io_ctx[io_ctx_nr]->iocb;
      iocbs[io_ctx_nr]->aio_data = io_ctx[io_ctx_nr];
      io_ctx_nr++;
      Assert(io_ctx_nr < AIO_MAX_EVENTS);
    }

    io_submit(aio_ctx, io_ctx_nr, iocbs);

    for (int i = 0; i < io_ctx_nr; ++i) {
      for (int j = 0; j < 4; ++j) {
        if (io_ctx[i]->do_at_io_wait[j] == NULL) {
          break;
        }
        io_ctx[i]->do_at_io_wait[j](io_ctx[i]);
      }
    }

    int ret = io_getevents(aio_ctx, io_ctx_nr, AIO_MAX_EVENTS, io_events, NULL);
    Assert(ret == io_ctx_nr);

    for (int i = 0; i < io_ctx_nr; ++i) {
      struct io_context* io_ctx = io_events[i].obj;
      io_ctx->event = &io_events[i];
      io_ctx->kv_event->return_code = io_events[i].res;
      for (int j = 0; j < 4; ++j) {
        if (io_ctx->do_at_io_finish[j] == NULL) {
          break;
        }
        io_ctx->do_at_io_finish[j](io_ctx);
      } // for
    } // for
  } // while
}