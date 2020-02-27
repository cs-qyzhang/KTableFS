#ifndef KTABLEFS_KVENGINE_IO_CONTEXT_H_
#define KTABLEFS_KVENGINE_IO_CONTEXT_H_

#include <stddef.h>
#include <linux/aio_abi.h>
#include <assert.h>

typedef void (*callback_t)(struct io_context*);

struct lru_entry;
struct kv_request;
struct kv_event;
struct pagecache;
struct thread_data;

struct io_context {
  // aio control blocks
  struct iocb* iocb;
  // aio return events
  struct io_event* event;

  struct thread_data* thread_data;
  struct lru_entry* lru;
  size_t page_offset;
  size_t slab_index;

  struct kv_request* kv_request;
  struct kv_event* kv_event;

  // functions called between io_submit() and io_getevent()
  // like string, NULL means end
  callback_t do_at_io_wait[4];
  // functions called after io_getevent()
  callback_t do_at_io_finish[4];
};

static inline void io_context_insert_callback(callback_t* callbacks,
                                              callback_t new_callback) {
  for (int i = 0; i < 4; ++i) {
    if (callbacks[i] == NULL) {
      callbacks[i] = new_callback;
      return;
    }
  }
  assert(0);
}

#endif // KTABLEFS_KVENGINE_IO_CONTEXT_H_