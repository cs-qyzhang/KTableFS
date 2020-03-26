#define _GNU_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <sched.h>
#include "util/queue.h"
#include "util/freelist.h"
#include "util/index.h"
#include "util/arena.h"
#include "util/memslab.h"
#include "kvengine/kvengine.h"
#include "kvengine/pagecache.h"
#include "kvengine/aio-wrapper.h"
#include "kvengine/io_context.h"
#include "kvengine/slab.h"
#include "kvengine/io_worker.h"
#include "ktablefs_config.h"
#include "debug.h"

#define KV_BATCH_MAX  10

struct kv_batch {
  struct kv_request* requests[KV_BATCH_MAX];
  int nr;
  int cur;
};

struct thread_data* threads_data;
pthread_t* threads;
int thread_nr;

static inline void nop() {
  for (volatile int i = 0; i < 1000; ++i) ;
  // static struct timespec req_tm = {.tv_sec = 0, .tv_nsec = 100};
  // nanosleep(&req_tm, NULL);
}

void io_context_enqueue(struct io_context* ctx) {
  pthread_mutex_lock(&ctx->thread_data->thread_lock);
  enqueue(ctx->thread_data->io_context_queue, ctx);
  pthread_mutex_unlock(&ctx->thread_data->thread_lock);
}

// callback function
void index_insert_wrapper(struct io_context* ctx) {
  index_insert(ctx->thread_data->index, keydup(ctx->batch->requests[ctx->batch->cur]->key),
      (void*)(uintptr_t)(ctx->slab_index + 1));
}

// callback function
void index_remove_wrapper(struct io_context* ctx) {
  index_remove(ctx->thread_data->index, ctx->batch->requests[ctx->batch->cur]->key);
}

// callback function
void kv_get_finish(struct io_context* ctx) {
  ctx->lru->valid = 1;
  void* item = &ctx->lru->page[ctx->page_offset];
  // item's first byte is valid flag, -1 means valid, 0 means deleted.
  Assert(((int8_t*)item)[0] == -1);
  item_to_value(item, &ctx->respond.value);
}

static __thread struct memslab io_context_memslab = {
  .arena     = NULL,
  .freelist  = NULL,
  .slab_size = sizeof(struct io_context)};

struct io_context* io_context_new(struct thread_data* thread_data,
    struct kv_batch* batch, int slab_idx) {

  struct io_context* ctx = memslab_alloc(&io_context_memslab);
  memset(ctx, 0, sizeof(*ctx));
  ctx->thread_data = thread_data;
  ctx->batch = batch;
  ctx->slab_index = slab_idx;
  return ctx;
}

void io_context_free(struct io_context* ctx) {
  memslab_free(&io_context_memslab, ctx);
}

void process_put_request(struct kv_batch* batch, struct thread_data* thread_data) {
  struct kv_request* req = batch->requests[batch->cur];

  void* val = index_lookup(thread_data->index, req->key);
  if (val) {
    // error! key already in!
    struct kv_respond* respond = malloc(sizeof(*respond));
    respond->res = -EEXIST;
    respond->value = NULL;
    kv_finish(batch, respond);
    free(respond);
    return;
  }

  char *item = malloc(512);
  size_t item_size = kv_to_item(req->key, req->value, item);
  int slab_idx = slab_get_free_index(thread_data->slab);

  struct io_context* ctx = io_context_new(thread_data, batch, slab_idx);
  io_context_insert_callback(ctx->do_at_io_wait, index_insert_wrapper);

  slab_write_item(thread_data->slab, slab_idx, item, item_size, ctx);
}

void process_get_request(struct kv_batch* batch, struct thread_data* thread_data) {
  struct kv_request* req = batch->requests[batch->cur];

  void* val = index_lookup(thread_data->index, req->key);
  if (!val) {
    struct kv_respond* respond = malloc(sizeof(*respond));
    respond->res = -ENOENT;
    respond->value = NULL;
    kv_finish(batch, respond);
    free(respond);
    return;
  }

  int slab_idx = (int)(uintptr_t)val - 1;

  struct io_context* ctx = io_context_new(thread_data, batch, slab_idx);
  io_context_insert_callback(ctx->do_at_io_finish, kv_get_finish);

  slab_read_item(thread_data->slab, slab_idx, ctx);
}

void process_update_request(struct kv_batch* batch, struct thread_data* thread_data) {
  struct kv_request* req = batch->requests[batch->cur];

  void* val = index_lookup(thread_data->index, req->key);
  if (!val) {
    struct kv_respond* respond = malloc(sizeof(*respond));
    respond->res = -ENOENT;
    respond->value = NULL;
    kv_finish(batch, respond);
    free(respond);
    return;
  }

  int slab_idx = (int)(uintptr_t)val - 1;

  char *item = malloc(512);
  size_t item_size = kv_to_item(req->key, req->value, item);

  struct io_context* ctx = io_context_new(thread_data, batch, slab_idx);

  slab_write_item(thread_data->slab, slab_idx, item, item_size, ctx);
}

void process_delete_request(struct kv_batch* batch, struct thread_data* thread_data) {
  struct kv_request* req = batch->requests[batch->cur];

  void* val = index_lookup(thread_data->index, req->key);
  if (!val) {
    struct kv_respond* respond = malloc(sizeof(*respond));
    respond->res = -ENOENT;
    respond->value = NULL;
    kv_finish(batch, respond);
    free(respond);
    return;
  }

  int slab_idx = (int)(uintptr_t)val - 1;

  // only change item's valid to 0
  int64_t *item_valid = malloc(sizeof(*item_valid));
  *item_valid = -1;

  struct io_context* ctx = io_context_new(thread_data, batch, slab_idx);
  io_context_insert_callback(ctx->do_at_io_wait, index_remove_wrapper);
  io_context_insert_callback(ctx->do_at_io_wait, slab_remove_item);

  slab_write_item(thread_data->slab, slab_idx, item_valid, sizeof(*item_valid), ctx);
}

void scan_request_function(void* key, void* value, void** scan_arg) {
  struct kv_request* req = scan_arg[0];
  struct thread_data* thread_data = scan_arg[1];
  int slab_idx = (int)(uintptr_t)value - 1;

  void* item = slab_read_item_sync(thread_data->slab, slab_idx);
  Assert(((int8_t*)item)[0] == -1);
  struct value* val;
  item_to_value(item, &val);
  req->scan(key, val, req->scan_arg);
}

void process_scan_request(struct kv_batch* batch, struct thread_data* thread_data) {
  struct kv_request* req = batch->requests[batch->cur];

  void** scan_arg = malloc(sizeof(void*) * 2);
  scan_arg[0] = req;
  scan_arg[1] = thread_data;
  int scan_nr = index_scan(thread_data->index, req->min_key, req->max_key,
                           scan_request_function, scan_arg);

  struct kv_respond* respond = malloc(sizeof(*respond));
  respond->res = scan_nr;
  respond->value = NULL;
  kv_finish(batch, respond);
  free(respond);
  free(scan_arg);
}

int worker_deque_request(struct thread_data* thread_data) {
  struct queue* kv_que = thread_data->kv_batch_queue;

  uint8_t cnt = 0;
  while (queue_empty(kv_que)) {
    if (++cnt == 0)
      pthread_testcancel();
    nop();
  }

  int req_nr = 0;
  while (!queue_empty(kv_que)) {
    struct kv_batch* batch = dequeue(kv_que);
    Assert(batch);
    switch (batch->requests[batch->cur]->type) {
      case PUT:    process_put_request(batch, thread_data); break;
      case GET:    process_get_request(batch, thread_data); break;
      case UPDATE: process_update_request(batch, thread_data); break;
      case DELETE: process_delete_request(batch, thread_data); break;
      case SCAN:   process_scan_request(batch, thread_data); break;
      default:     Assert(0); break;
    }
    req_nr++;
  }
  return req_nr;
}

static void pin_me_on(int core) {
  cpu_set_t cpuset;
  pthread_t thread = pthread_self();

  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);

  int s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (s != 0)
    printf("Cannot pin thread on core %d\n", core);
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

  // pin_me_on(thread_data->thread_index);

  while (1) {
    worker_deque_request(thread_data);
    if (queue_empty(io_que))
      continue;

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
        if (io_ctx[i]->do_at_io_wait[j] == NULL)
          break;
        io_ctx[i]->do_at_io_wait[j](io_ctx[i]);
      }
    }

    int ret = io_getevents(aio_ctx, io_ctx_nr, AIO_MAX_EVENTS, io_events, NULL);
    assert(ret == io_ctx_nr);

    // do io finish callback
    for (int i = 0; i < io_ctx_nr; ++i) {
      struct io_context* io_ctx = (void*)io_events[i].data;
      io_ctx->event = &io_events[i];
      io_ctx->respond.res = io_events[i].res;
      for (int j = 0; j < 4; ++j) {
        if (io_ctx->do_at_io_finish[j] == NULL)
          break;
        io_ctx->do_at_io_finish[j](io_ctx);
      } // for
      kv_finish(io_ctx->batch, &io_ctx->respond);
      io_context_free(io_ctx);
    } // for
  } // while
}

void thread_data_init(struct thread_data* data, int thread_idx, struct kv_options* option) {
  pthread_mutex_init(&data->thread_lock, NULL);
  data->kv_batch_queue = queue_new(128);
  data->index = index_new(key_comparator);
  data->io_context_queue = queue_new(64);
  data->arena = arena_new();
  data->pagecache = pagecache_new(PAGECACHE_NR_PAGE);
  data->thread_index = thread_idx;
  data->slab = malloc(sizeof(struct slab));
  data->slab->slab_size = 256;
  data->slab->used = 0;
  data->slab->max_index = 0;
  data->slab->arena = data->arena;
  data->slab->pgcache = data->pagecache;
  data->slab->freelist = freelist_new(data->arena);
  char slab_file_name[1024];
  sprintf(slab_file_name, "%s/slab-%d", option->slab_dir, thread_idx);
  data->slab->fd = open(slab_file_name, O_CREAT | O_RDWR, 0644);
  if (data->slab->fd == -1) {
    perror(strerror(data->slab->fd));
    assert(0);
  }
}

void io_worker_init(struct kv_options* option) {
  thread_nr = option->thread_nr;
  threads = malloc(sizeof(pthread_t) * thread_nr);
  threads_data = malloc(sizeof(struct thread_data) * thread_nr);

  for (int i = 0; i < thread_nr; ++i) {
    thread_data_init(&threads_data[i], i, option);
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

void kv_init(struct kv_options* options) {
  io_worker_init(options);
}

void kv_destroy() {
  io_worker_destroy();
}

static __thread struct memslab batch_memslab = {
  .arena     = NULL,
  .freelist  = NULL,
  .slab_size = sizeof(struct kv_batch)};
static __thread struct memslab request_memslab = {
  .arena     = NULL,
  .freelist  = NULL,
  .slab_size=sizeof(struct kv_request)};

void kv_submit(struct kv_request* reqs, int nr) {
  assert(nr <= KV_BATCH_MAX && nr > 0);

  int thread_idx = get_thread_index(reqs[0].key, thread_nr);

  struct kv_batch* batch = memslab_alloc(&batch_memslab);
  batch->cur = 0;
  batch->nr = nr;

  for (int i = 0; i < nr; ++i) {
    batch->requests[i] = memslab_alloc(&request_memslab);
    memcpy(batch->requests[i], &reqs[i], sizeof(struct kv_request));
    if (reqs[i].key)
      batch->requests[i]->key = keydup(reqs[i].key);
    if (reqs[i].value)
      batch->requests[i]->value = valuedup(reqs[i].value);
    if (reqs[i].max_key)
      batch->requests[i]->max_key = keydup(reqs[i].max_key);
  }

  pthread_mutex_lock(&threads_data[thread_idx].thread_lock);
  enqueue(threads_data[thread_idx].kv_batch_queue, batch);
  pthread_mutex_unlock(&threads_data[thread_idx].thread_lock);
}

void kv_finish(struct kv_batch* batch, struct kv_respond* respond) {
  struct kv_request* req = batch->requests[batch->cur];

  if (req->callback)
    req->callback(req->userdata, respond);

  if (req->key)
    keydup_free(req->key);
  if (req->value)
    valuedup_free(req->value);
  if (req->max_key)
    keydup(req->max_key);
  memslab_free(&request_memslab, req);

  if (++batch->cur < batch->nr) {
    int thread_idx = get_thread_index(batch->requests[batch->cur]->key, thread_nr);
    pthread_mutex_lock(&threads_data[thread_idx].thread_lock);
    enqueue(threads_data[thread_idx].kv_batch_queue, batch);
    pthread_mutex_unlock(&threads_data[thread_idx].thread_lock);
  } else {
    memslab_free(&batch_memslab, batch);
  }
}