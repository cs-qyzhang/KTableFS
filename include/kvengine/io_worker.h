#ifndef KTABLEFS_KVENGINE_IO_WORKER_H_
#define KTABLEFS_KVENGINE_IO_WORKER_H_

#include "kvengine/aio-wrapper.h"

struct queue;
struct index;
struct arena;
struct slab;
struct pagecache;

struct thread_data {
  struct queue* kv_request_queue;
  struct index* index;
  struct queue* kv_event_queue;
  struct queue* io_context_queue;
  struct arena* arena;
  struct slab* slab;
  struct pagecache* pagecache;
  aio_context_t aio_ctx;
  int max_sequence;
};

#endif // KTABLEFS_KVENGINE_IO_WORKER_H_