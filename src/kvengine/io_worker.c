#define _GNU_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "util/queue.h"
#include "util/freelist.h"
#include "util/index.h"
#include "util/arena.h"
#include "kvengine/kvengine.h"
#include "kvengine/pagecache.h"
#include "kvengine/aio-wrapper.h"
#include "kvengine/io_context.h"
#include "kvengine/slab.h"
#include "kvengine/io_worker.h"
#include "ktablefs_config.h"

#ifdef DEBUG
#include <assert.h>
#define Assert(expr)  assert(expr)
#else // DEBUG
#define Assert(expr)
#endif

struct thread_data* threads_data;
pthread_t* threads;
int thread_nr;

static inline void nop() {
  for (volatile int i = 0; i < 10000; ++i) ;
}

struct kv_event* kv_event_new() {
  struct kv_event* event = malloc(sizeof(*event));
  event->value = NULL;
  return event;
}

void io_context_enqueue(struct io_context* ctx) {
  enqueue(ctx->thread_data->io_context_queue, ctx);
}

void kv_event_enqueue(struct kv_event* event, struct thread_data* thread_data) {
  enqueue(thread_data->kv_event_queue, event);
}

// callback function
void index_insert_wrapper(struct io_context* ctx) {
  index_insert(ctx->thread_data->index, ctx->kv_request->key, (void*)(uintptr_t)(ctx->slab_index + 1));
}

// callback function
void index_remove_wrapper(struct io_context* ctx) {
  index_remove(ctx->thread_data->index, ctx->kv_request->key);
}

// callback function
void kv_put_finish(struct io_context* ctx) {
  ctx->kv_event->return_code = 0;
  kv_event_enqueue(ctx->kv_event, ctx->thread_data);
}

// callback function
void kv_get_finish(struct io_context* ctx) {
  ctx->lru->valid = 1;
  void* item = &ctx->lru->page[ctx->page_offset];
  ctx->kv_event->value = malloc(value_size());
  // item's first byte is valid flag, -1 means valid, 0 means deleted.
  Assert(((int8_t*)item)[0] == -1);
  item_to_kv(item, NULL, ctx->kv_event->value);
  ctx->kv_event->return_code = 0;
  kv_event_enqueue(ctx->kv_event, ctx->thread_data);
}

// callback function
void kv_update_finish(struct io_context* ctx) {
  ctx->kv_event->return_code = 0;
  kv_event_enqueue(ctx->kv_event, ctx->thread_data);
}

// callback function
void kv_delete_finish(struct io_context* ctx) {
  ctx->kv_event->return_code = 0;
  kv_event_enqueue(ctx->kv_event, ctx->thread_data);
}

struct io_context* io_context_new(struct thread_data* thread_data, struct kv_request* req, int slab_idx) {
  struct io_context* ctx = malloc(sizeof(*ctx));
  memset(ctx, 0, sizeof(*ctx));
  ctx->thread_data = thread_data;
  ctx->kv_request = req;
  ctx->kv_event = kv_event_new();
  ctx->kv_event->sequence = ctx->kv_request->sequence;
  ctx->slab_index = slab_idx;
  return ctx;
}

void process_put_request(struct kv_request* req, struct thread_data* thread_data) {
  void* val = index_lookup(thread_data->index, req->key);
  if (val) {
    // error! key already in!
    struct kv_event* event = kv_event_new();
    event->sequence = req->sequence;
    event->return_code = -1;
    kv_event_enqueue(event, thread_data);
    return;
  }

  void* item;
  size_t item_size = kv_to_item(req->key, req->value, &item);
  int slab_idx = slab_get_free_index(thread_data->slab);

  struct io_context* ctx = io_context_new(thread_data, req, slab_idx);
  io_context_insert_callback(ctx->do_at_io_wait, index_insert_wrapper);
  io_context_insert_callback(ctx->do_at_io_finish, kv_put_finish);

  slab_write_item(thread_data->slab, slab_idx, item, item_size, ctx);
}

void process_get_request(struct kv_request* req, struct thread_data* thread_data) {
  void* val = index_lookup(thread_data->index, req->key);
  if (!val) {
    struct kv_event* event = kv_event_new();
    event->sequence = req->sequence;
    event->return_code = -1;
    enqueue(thread_data->kv_event_queue, event);
    return;
  }

  int slab_idx = (int)(uintptr_t)val - 1;

  struct io_context* ctx = io_context_new(thread_data, req, slab_idx);
  io_context_insert_callback(ctx->do_at_io_finish, kv_get_finish);

  slab_read_item(thread_data->slab, slab_idx, ctx);
}

void process_update_request(struct kv_request* req, struct thread_data* thread_data) {
  void* val = index_lookup(thread_data->index, req->key);
  if (!val) {
    struct kv_event* event = kv_event_new();
    event->sequence = req->sequence;
    event->return_code = -1;
    enqueue(thread_data->kv_event_queue, event);
    return;
  }

  int slab_idx = (int)(uintptr_t)val - 1;

  void* item;
  size_t item_size = kv_to_item(req->key, req->value, &item);

  struct io_context* ctx = io_context_new(thread_data, req, slab_idx);
  io_context_insert_callback(ctx->do_at_io_finish, kv_update_finish);

  slab_write_item(thread_data->slab, slab_idx, item, item_size, ctx);
}

void process_delete_request(struct kv_request* req, struct thread_data* thread_data) {
  void* val = index_lookup(thread_data->index, req->key);
  if (!val) {
    struct kv_event* event = kv_event_new();
    event->sequence = req->sequence;
    event->return_code = -1;
    enqueue(thread_data->kv_event_queue, event);
    return;
  }

  int slab_idx = (int)(uintptr_t)val - 1;

  // only change item's valid to 0
  char* item = malloc(1);
  item[0] = 0;

  struct io_context* ctx = io_context_new(thread_data, req, slab_idx);
  io_context_insert_callback(ctx->do_at_io_wait, index_remove_wrapper);
  io_context_insert_callback(ctx->do_at_io_wait, slab_remove_item);
  io_context_insert_callback(ctx->do_at_io_finish, kv_delete_finish);

  slab_write_item(thread_data->slab, slab_idx, item, 1, ctx);
}

int worker_deque_request(struct thread_data* thread_data) {
  struct queue* kv_que = thread_data->kv_request_queue;

  while (queue_empty(kv_que)) {
    pthread_testcancel();
    nop();
  }

  int req_nr = 0;
  while (!queue_empty(kv_que)) {
    struct kv_request* req = dequeue(kv_que);
    Assert(req);
    switch (req->type) {
      case PUT:
        process_put_request(req, thread_data);
        break;
      case GET:
        process_get_request(req, thread_data);
        break;
      case UPDATE:
        process_update_request(req, thread_data);
        break;
      case DELETE:
        process_delete_request(req, thread_data);
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
  struct thread_data* thread_data = arg;

  struct io_context* io_ctx[AIO_MAX_EVENTS];
  struct iocb** iocbs = malloc(sizeof(struct iocb*) * AIO_MAX_EVENTS);
  struct io_event* io_events = malloc(sizeof(struct io_event) * AIO_MAX_EVENTS);
  struct queue* io_que = thread_data->io_context_queue;

  aio_context_t aio_ctx;
  io_setup(AIO_MAX_EVENTS, &aio_ctx);
  thread_data->aio_ctx = aio_ctx;

  while (1) {
    worker_deque_request(thread_data);
    if (queue_empty(io_que)) {
      continue;
    }

    int io_ctx_nr = 0;
    while (!queue_empty(io_que)) {
      io_ctx[io_ctx_nr] = dequeue(io_que);
      iocbs[io_ctx_nr] = io_ctx[io_ctx_nr]->iocb;
      iocbs[io_ctx_nr]->aio_data = (uintptr_t)io_ctx[io_ctx_nr];
      io_ctx_nr++;
      Assert(io_ctx_nr < AIO_MAX_EVENTS);
    }

    io_submit(aio_ctx, io_ctx_nr, iocbs);

    // do io wait callback
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

    // do io finish callback
    for (int i = 0; i < io_ctx_nr; ++i) {
      struct io_context* io_ctx = (void*)io_events[i].data;
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

void thread_data_init(struct thread_data* data, int thread_idx, struct option* option) {
  data->kv_request_queue = queue_new(64);
  data->index = index_new(key_comparator);
  data->kv_event_queue = queue_new(64);
  data->io_context_queue = queue_new(64);
  data->arena = arena_new();
  data->pagecache = pagecache_new(256);
  data->max_sequence = 0;
  data->slab = malloc(sizeof(struct slab));
  data->slab->slab_size = 256;
  data->slab->used = 0;
  data->slab->max_index = 0;
  data->slab->arena = data->arena;
  data->slab->pgcache = data->pagecache;
  data->slab->freelist = freelist_new(data->arena);
  char slab_file_name[512];
  sprintf(slab_file_name, "%s/slab-%d", option->slab_dir, thread_idx);
  data->slab->fd = open(slab_file_name, O_CREAT | O_RDWR, 0777);
  if (data->slab->fd == -1) {
    perror(strerror(data->slab->fd));
    assert(0);
  }
}

void io_worker_init(struct option* option) {
  thread_nr = option->thread_nr;
  threads = malloc(sizeof(pthread_t) * thread_nr);
  threads_data = malloc(sizeof(struct thread_data) * thread_nr);

  for (int i = 0; i < thread_nr; ++i) {
    thread_data_init(&threads_data[i], i, option);
  }

  for (int i = 0; i < thread_nr; ++i) {
    pthread_create(&threads[i], NULL, worker_thread_main, &threads_data[i]);
  }
}

void io_worker_destroy() {
  for (int i = 0; i < thread_nr; ++i) {
    pthread_cancel(threads[i]);
    pthread_join(threads[i], NULL);
    close(threads_data[i].slab->fd);
    io_destroy(threads_data[i].aio_ctx);
  }
}

int kv_submit(struct kv_request* request) {
  int thread_idx = get_thread_index(request->key, thread_nr);

  // should make a copy of user request, because user may
  // use this request for further submit
  struct kv_request* dup_req = malloc(sizeof(*dup_req));
  memcpy(dup_req, request, sizeof(*request));
  if (request->key) {
    dup_req->key = malloc(key_size());
    memcpy(dup_req->key, request->key, key_size());
  }
  if (request->value) {
    dup_req->value = malloc(value_size());
    memcpy(dup_req->value, request->value, value_size());
  }
  if (request->max_key) {
    dup_req->max_key = malloc(key_size());
    memcpy(dup_req->max_key, request->max_key, key_size());
  }

  // TODO: lock
  int sequence = (threads_data[thread_idx].max_sequence++) | (thread_idx << 24);
  request->sequence = sequence;
  dup_req->sequence = sequence;
  enqueue(threads_data[thread_idx].kv_request_queue, dup_req);

  return sequence;
}

struct kv_event* kv_getevent(int sequence) {
  int thread_idx = sequence >> 24;
  while (queue_empty(threads_data[thread_idx].kv_event_queue)) {
    nop();
  }
  return dequeue(threads_data[thread_idx].kv_event_queue);
}