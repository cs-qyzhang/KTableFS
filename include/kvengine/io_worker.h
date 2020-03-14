#ifndef KTABLEFS_KVENGINE_IO_WORKER_H_
#define KTABLEFS_KVENGINE_IO_WORKER_H_

#include <pthread.h>
#include "kvengine/aio-wrapper.h"

struct queue;
struct index;
struct arena;
struct slab;
struct pagecache;

struct thread_data {
  pthread_mutex_t thread_lock;
  struct queue* kv_batch_queue;
  struct index* index;
  struct queue* io_context_queue;
  struct arena* arena;
  struct slab* slab;
  struct pagecache* pagecache;
  aio_context_t aio_ctx;
  int max_sequence;
};

#endif // KTABLEFS_KVENGINE_IO_WORKER_H_