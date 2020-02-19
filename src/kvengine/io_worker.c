#include <stdlib.h>
#include "kvengine/io_context.h"
#include "kvengine/queue.h"
#include "kvengine/aio-wrapper.h"
#include "ktablefs_config.h"

struct queue* io_que;
struct io_context* io_context;

static inline void nop() {
  for (volatile int i = 0; i < 1000; ++i) ;
}

void* worker_thread_main(void* arg) {
  int thread_idx = *(int*)arg;

  aio_context_t aio_ctx;
  io_setup(AIO_MAX_EVENTS, &aio_ctx);

  while (1) {
    while (queue_empty(io_que)) {
      nop();
    }

    struct io_context* io_ctx = dequeue(io_que);
    Assert(io_ctx);
    Assert(io_ctx->nr < AIO_MAX_EVENTS);
    io_ctx->events = NULL;

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