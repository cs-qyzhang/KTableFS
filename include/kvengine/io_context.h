#ifndef KTABLEFS_KVENGINE_IO_CONTEXT_H_
#define KTABLEFS_KVENGINE_IO_CONTEXT_H_

#include <sys/types.h>
#include <linux/aio_abi.h>

typedef void (*callback_t)(struct io_context*);

struct io_context {
  // aio control blocks
  struct iocb** iocbs;
  // aio return events
  struct io_event* events;
  // sequence number
  size_t sequence;
  // number of iocbs
  size_t nr;
  // function called between io_submit() and io_getevent()
  callback_t do_at_io_wait;
  // function called after io_getevent()
  callback_t do_at_io_finish;
};

#endif // KTABLEFS_KVENGINE_IO_CONTEXT_H_