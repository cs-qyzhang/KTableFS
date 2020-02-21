#include <stdlib.h>
#include "arena.h"
#include "kvengine/queue.h"
#include "kvengine/index.h"
#include "kvengine/aio-wrapper.h"
#include "kvengine/kv_request.h"
#include "kvengine/kv_event.h"
#include "kvengine/slab.h"
#include "ktablefs_config.h"

struct queue** kv_request_queues;
struct index** indexs;
struct queue** kv_event_queues;
struct arena** arenas;

typedef void (*callback_t)(struct io_callback*);

struct io_callback {
  size_t sequence;
  // function called between io_submit() and io_getevent()
  callback_t do_at_io_wait;
  // function called after io_getevent()
  callback_t do_at_io_finish;
};

static inline void nop() {
  for (volatile int i = 0; i < 1000; ++i) ;
}

int process_kv_request(struct kv_request* req, int thread_idx) {
  struct queue* event_que = kv_event_queues[thread_idx];
  struct index* index = indexs[thread_idx];
  struct arena* arena = arenas[thread_idx];
  Assert(index && event_que && arena);

  switch (req->type) {
    case ADD:
      void* val = index_lookup(index, req->key);
      if (val) {
        // error! key already in!
        struct kv_event* event = malloc(sizeof(*event));
        event->sequence = req->sequence;
        event->return_code = -1;
        enqueue(event_que, event);
      } else {
        int slab_idx = slab_get_index();
        size_t item_size = kv_to_item(req->key, req->key_size, req->value, req->value_size, &item);
        struct io_callback* io_callback = malloc(sizeof(*io_callback));
        io_callback->sequence = req->sequence;
        io_callback->do_at_io_wait = ;
        io_callback->do_at_io_finish = ;
        slab_write_item(slab, slab_idx, item, item_size, io_callback);
      }
      break;
    case UPDATE:
      break;
    case DELETE:
      break;
    default:
      Assert(0);
  }
}

int worker_deque_request(int thread_idx) {
  struct queue* io_que = kv_request_queues[thread_idx];
  Assert(io_que);

  while (queue_empty(io_que)) {
    nop();
  }

  int req_nr = 0;
  while (!queue_empty(io_que)) {
    struct kv_request* kv_req = dequeue(io_que);
    Assert(kv_req);
    req_nr++;
  }
}

void* worker_thread_main(void* arg) {
  int thread_idx = *(int*)arg;

  aio_context_t aio_ctx;
  io_setup(AIO_MAX_EVENTS, &aio_ctx);

  while (1) {

    // TODO: error handle
    io_submit(aio_ctx, io_ctx->nr, io_ctx->iocbs);

    if (io_ctx->do_at_io_wait) {
      io_ctx->do_at_io_wait(io_ctx);
    }

    io_ctx->events = malloc(sizeof(struct io_event) * AIO_MAX_EVENTS);
    // TODO: error handle
    io_getevents(aio_ctx, io_ctx->nr, AIO_MAX_EVENTS, io_context->events, NULL);

    if (io_ctx->do_at_io_finish) {
      io_ctx->do_at_io_finish(io_ctx);
    }
  }
}